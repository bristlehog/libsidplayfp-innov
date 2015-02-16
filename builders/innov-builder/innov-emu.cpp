/*
* Innovation SSI-2001 port of libsidplayfp
* This file is part of libsidplayfp, a SID player engine.
*
* Copyright 2015 Boris Chuprin <ej@tut.by>
* Copyright 2014-2015 Pavel Ageev <pageev@mail.ru>
* Copyright 2011-2013 Leandro Nini <drfiemost@users.sourceforge.net>
* Copyright 2007-2010 Antti Lankila
* Copyright 2001 Simon White
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

const unsigned int Innov::voices = INNOV_VOICES;
unsigned Innov::sid = 0;

static unsigned timer_refcount;

static void timer_grab(void);
static void timer_release(void);

u32 vcpu_freq;

Innov::Innov(sidbuilder *builder) :
sidemu(builder),
Event("Innov Delay")
{
    ++sid;

    timer_grab();
    /*
    m_error.assign("HARDSID ERROR: Cannot access \"").append(device).append("\"");
    return;
    */

    m_status = true;
    reset();
}

Innov::~Innov()
{
    //printf("_DEBUG: Innov::~Innov call!\n");
    timer_release();
    --sid;
}

void Innov::voice(unsigned int num, bool mute)
{
    //printf("_DEBUG: Innov::voice call!\n");
    //printf("_DEBUG: Innov::voice | voice = %u, mute = %u\n", num, mute);    

    // Only have 3 voices!
    if (num >= voices)
        return;

    muted[num] = mute;

    int cmute = 0;
    for (unsigned int i = 0; i < voices; i++) {
        cmute |= (muted[i] << i);
    }
}

void Innov::filter(bool enable)
{
    //printf("_DEBUG: Innov::filter call!\n");

    //ioctl(m_handle, HSID_IOCTL_NOFILTER, !enable);
}

void Innov::reset(uint8_t volume)
{
    //printf("_DEBUG: Innov::reset call!\n");

    for (unsigned int i = 0; i < voices; i++)
        muted[i] = false;

    for (unsigned int i = 0; i < 24; i++) {
        out(i, 0);
    }

    m_accessClk = 0;
    reset_timer();
    if (m_context != 0)
        m_context->schedule(*this, HARDSID_DELAY_CYCLES, EVENT_CLOCK_PHI1);
}


uint8_t Innov::read(uint_least8_t addr)
{
    //printf("_DEBUG: Innov::read call!\n");

    /* we don't need HardSID's handle
    if (!m_handle)
    {
    printf("_DEBUG: Innov::read | m_handle is false, exiting.\n");
    return 0;
    }
    */

    m_accessClk += (u32)m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);

    //printf("_DEBUG: Innov::read | cycles = %lu\n", cycles);    

    synchronize();

    //ioctl(m_handle, HSID_IOCTL_READ, &packet);

    unsigned int packet = in(addr);

    return (uint8_t)packet;
}


void Innov::write(uint_least8_t addr, uint8_t data)
{
    //printf("_DEBUG: Innov::write call!\n");

    m_accessClk += (u32)m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);

    //printf("_DEBUG: Innov::write | cycles = %lu\n", cycles);

    synchronize();

    out(addr, data);
}

bool Innov::lock(EventContext* env)
{
    //printf("_DEBUG: Innov::lock call!\n");

    sidemu::lock(env);
    m_context->schedule(*this, HARDSID_DELAY_CYCLES, EVENT_CLOCK_PHI1);
    return true;
}

void Innov::unlock()
{
    //printf("_DEBUG: Innov::unlock call!\n");

    m_context->cancel(*this);
    sidemu::unlock();
}

void Innov::sampling(float systemfreq, float outputfreq,
    SidConfig::sampling_method_t method, bool fast)
{
    vcpu_freq = (u32)systemfreq;
    reset_timer();
    // All other parameters ignored
}


void Innov::synchronize()
{
    m_accessClk += (u32)m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);
    wait();
}

void Innov::event()
{

    //printf("_DEBUG: Innov::event call!\n");

    event_clock_t cycles = m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);
    if (cycles < 60000)
    {
        m_context->schedule(*this, 60000 - cycles,
            EVENT_CLOCK_PHI1);
    }
    else
    {
        synchronize();
        m_context->schedule(*this, 60000, EVENT_CLOCK_PHI1);
    }
}

void Innov::clock()
{
    //printf("_DEBUG: Innov::clock call!\n");

    synchronize();
}

static void timer_grab(void)
{
    if (0 == timer_refcount++) {
        timer_create();
    }
}

static void timer_release(void)
{
    if (0 == timer_refcount) {
        // TODO: LOG Error!!!!!
        return;
    }
    if (0 == --timer_refcount) {
        timer_free();
    }
}