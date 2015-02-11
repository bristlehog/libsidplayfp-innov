/*
* Windows 9X - specific code for Innovation SSI-2001 port of libsidplayfp
* This file is part of libsidplayfp, a SID player engine.
*
* Copyright 2015 Boris Chuprin <ej@tut.by>
* Copyright 2014-2015 Pavel Ageev <pageev@mail.ru>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#define _CRT_NONSTDC_NO_DEPRECATE

#include "innov-emu.h"
#include "innov.h"

#include <string>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <dos.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif


typedef LARGE_INTEGER timer64_t;

static timer64_t timer_freq;

static timer64_t timer_tick;
static timer64_t timer_tick_base;
static u32 vcpu_tick_base;

// This scaling factor has 14.18 unsigned fixed-point format in order to balance wide range of possible timer frequencies with acceptable conversion precision
static u32 timer_freq_scale;        


u8 Innov::in(unsigned port)
{
    return (u8)inp(port + INNOV_PORT);
}


void Innov::out(unsigned port, u8 data)
{
    outp(port + INNOV_PORT, data);
}


void timer_create(void)
{
    QueryPerformanceFrequency(&timer_freq);
}


void timer_free(void)
{
    // Do nothing
}


void Innov::wait()
{
    u32 vcpu_tick_target = (u32)m_accessClk - vcpu_tick_base;
    static timer64_t timer_tick_target;

    // Adjust base
    while (vcpu_tick_target > vcpu_freq) {
        vcpu_tick_target -= vcpu_freq;
        vcpu_tick_base += vcpu_freq;
        timer_tick_base.QuadPart += timer_freq.QuadPart;
    }

    // timer_ticks_target = timer_tick_base + vcpu_tick_target + (vcpu_tick_target * V_SCALE >> 18);
    __asm {
        mov eax, [vcpu_tick_target]
        mul [timer_freq_scale]
        shrd eax, edx, 18
        shr  edx, 18
        add eax, DWORD PTR [timer_tick_base]
        add edx, DWORD PTR[timer_tick_base + 4]
        mov DWORD PTR [timer_tick_target], eax
        mov DWORD PTR [timer_tick_target + 4], edx
    }

    QueryPerformanceCounter(&timer_tick);
    while ((timer_tick.QuadPart - timer_tick_target.QuadPart) < 0) {
        // Spin
        QueryPerformanceCounter(&timer_tick);
    }
}


bool Innov::reset_timer()
{
    vcpu_tick_base = (u32)m_accessClk;
    QueryPerformanceCounter(&timer_tick);
    timer_tick_base.QuadPart = timer_tick.QuadPart;

    if (0 == vcpu_freq) {
        m_error = "ERROR: Innov::reset_timer() : system frequency not specified";
        vcpu_freq = 1000000;
        return false;
    }

    // Generate scaling factor using x86 long division
    // timer_freq_scale = ((0x100000000ULL * (PC_TIMER_FREQ - vcpu_freq)) / vcpu_freq)

    //timer_freq_scale = (timer_freq.x << 18) / vcpu_freq;
    __asm {
        mov eax, DWORD PTR[timer_freq]
        mov edx, DWORD PTR[timer_freq + 4]
        shld edx, eax, 18
        shl eax, 18
        div [vcpu_freq]
        mov [timer_freq_scale], eax
    }

    printf("_DEBUG: event_frequency: %lu freq_scale = %lf\n", vcpu_freq, timer_freq_scale / (double)(1 << 18));
    return true;
}
