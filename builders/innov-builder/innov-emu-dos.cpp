/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2014 Leandro Nini <drfiemost@users.sourceforge.net>
 * Copyright 2007-2010 Antti Lankila
 * Copyright 2001-2001 by Jarno Paananen
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

#include <stdint.h>
#include <cstdio>
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <dos.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define NTSC_MASTER_CLOCK 14318180UL
#define PC_TIMER_DIVIDER 16
#define PC_TIMER_FREQ (NTSC_MASTER_CLOCK/12)

#define PC_TIMER_INTERRUPT_VECTOR (0x8)

volatile static u32 timer_tick;
static const u32 timer_freq = PC_TIMER_FREQ;
static u32 timer_tick_base;
static u32 timer_freq_scale;
static u32 vcpu_freq;
static u32 vcpu_tick_base;
    // TODO: Support several instances later?


static unsigned timer_refcount;

const unsigned int Innov::voices = INNOV_VOICES;
unsigned Innov::sid = 0;



/*

// Move these to common header file
#define HSID_IOCTL_RESET     _IOW('S', 0, int)
#define HSID_IOCTL_FIFOSIZE  _IOR('S', 1, int)
#define HSID_IOCTL_FIFOFREE  _IOR('S', 2, int)
#define HSID_IOCTL_SIDTYPE   _IOR('S', 3, int)
#define HSID_IOCTL_CARDTYPE  _IOR('S', 4, int)
#define HSID_IOCTL_MUTE      _IOW('S', 5, int)
#define HSID_IOCTL_NOFILTER  _IOW('S', 6, int)
#define HSID_IOCTL_FLUSH     _IO ('S', 7)
#define HSID_IOCTL_DELAY     _IOW('S', 8, int)
#define HSID_IOCTL_READ      _IOWR('S', 9, int*)

*/


void (__interrupt * old_timer_isr)(void);

static void __interrupt timer_isr(void)
{
    timer_tick += PC_TIMER_DIVIDER;
    if(0 == (timer_tick & 0xFFFF)) {
        old_timer_isr();             /* Chain back to original interrupt */
    } else {
        __asm {
            mov al, 0x20
            out 0x20, al
        }
    }
}

/*  Ordinarily using INT 01Ch is the preferred method, but INT 08h can
 *  also be used.
 */


static void timer_grab(void)
{
    if(0 == timer_refcount++) {
        old_timer_isr = _dos_getvect(PC_TIMER_INTERRUPT_VECTOR);
        _dos_setvect(PC_TIMER_INTERRUPT_VECTOR, timer_isr);
        __asm {
            cli                               ;Disabled interrupts (just in case)
            
            mov al,00110100b                  ;channel 0, lobyte/hibyte, rate generator
            out 0x43, al
            
            mov ax, PC_TIMER_DIVIDER          ;ax = 16 bit reload value
            out 0x40, al                      ;Set low byte of PIT reload value
            mov al, ah                        ;ax = high 8 bits of reload value
            out 0x40, al
            
            sti
        }
    }
}

static void timer_release(void)
{
    if (0 == timer_refcount) {
            // TODO: LOG Error!!!!!
        return;
    }
    if(0 == --timer_refcount) {
        __asm {
            cli                               ;Disabled interrupts (just in case)
            
            mov al,00110100b                  ;channel 0, lobyte/hibyte, rate generator
            out 0x43, al
            
            xor eax, eax                      
            out 0x40, al                      ;Set low byte of PIT reload value
            out 0x40, al
            
            sti
        }
        _dos_setvect(PC_TIMER_INTERRUPT_VECTOR, old_timer_isr);
    }
}

#if 0
unsigned read_8254_count (void)
{
    unsigned short result;
    
    __asm
    {
        pushf             // save current inetrrupt status
        cli               // no ints while we read the 8254
        mov  al,0         // command to latch counter for counter 0
        out  0x43,al      // tell the 8254 to latch the count
        db   0x24h, 0xf0  // jmp $+2, this slow I/O on fast processors
        in   al,0x40      // read LSB of 8254's count
        mov  ah,al        // temp save
        db   0x24, 0xf0   // jmp $+2
        in   al,0x40      // read MSB of count
        xchg al,ah        // get right order of MSB/LSB
        popf              // return saved interrupt status
        mov result, ax
    }
    return result;
}

void start_timer(void)
{
    unsigned short cnt;

    // setting timer channel to 2
    outp(0x43, 0xb6);
        
    // computing delay to load into timer counter register
    cnt = 1; // (unsigned short)(1193180L / 985248L);
        
    // setting timer counter register
    /*outp(0x42, cnt & 0x00ff);
    outp(0x42, (cnt &0xff00) >> 8);*/

    outp(0x42, 1);
    outp(0x42, 0);
}


void tm_delay(unsigned short ticks) 
{
    _asm 
    {
        push si
        
        mov  si, ticks
        mov  ah, 0
        int  1ah
        
        mov  bx, dx
        add  bx, si
        
    delay_loop:
        
        int  1ah
        cmp  dx, bx
        jne  delay_loop
        
        pop  si
    }
        
}

void cycle_delay(unsigned short delay_cycles)
{
    float correction = 1.211;
    float temp_cycles;
    
    unsigned short dcycles;

    temp_cycles = delay_cycles / correction;
    dcycles = ceil(temp_cycles);
    
    
    start_timer();
    tm_delay(dcycles);
}

#endif
const char* Innov::getCredits()
{
    if (m_credit.empty())
    {
        // Setup credits
        /*std::ostringstream ss;
        ss << "HardSID V" << VERSION << " Engine:\n";
        ss << "\t(C) 2001-2002 Jarno Paanenen\n";
        m_credit = ss.str();*/
        char ss[128];
        
        sprintf(ss, "Innovation SSI-2001 Engine v. 1.0\n");
        
        m_credit = std::string(ss, 128);
    }   

    return m_credit.c_str();
}

Innov::Innov (sidbuilder *builder) :
    sidemu(builder),
    Event("Innov Delay")
{
    ++sid;

    timer_grab();
/*
    {
        char device[20];
        sprintf (device, "/dev/sid%u", m_instance);
        m_handle = open (device, O_RDWR);
        if (m_handle < 0)
        {
            if (m_instance == 0)
            {
                m_handle = open ("/dev/sid", O_RDWR);
                if (m_handle < 0)
                {
                    m_error.assign("HARDSID ERROR: Cannot access \"/dev/sid\" or \"").append(device).append("\"");
                    return;
                }
            }
            else
            {
                m_error.assign("HARDSID ERROR: Cannot access \"").append(device).append("\"");
                return;
            }
        }
    }
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

void Innov::reset(uint8_t volume)
{
    //printf("_DEBUG: Innov::reset call!\n");
    
    for (unsigned int i= 0; i < voices; i++)
        muted[i] = false;
    //ioctl(m_handle, HSID_IOCTL_RESET, volume);

    for(unsigned int i = 0; i < 24; i++)
    {
        outp(INNOV_PORT + i, 0);
    }
    
    m_accessClk = 0;
    if (m_context != 0)
        m_context->schedule(*this, HARDSID_DELAY_CYCLES, EVENT_CLOCK_PHI1);
}

void Innov::clock()
{

    unsigned long clk_counter;
    //printf("_DEBUG: Innov::clock call!\n");
    
    synchronize();
}

void Innov::synchronize() 
{
    event_clock_t cycles = (u32)m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);
    u32 vcpu_tick_target = (u32)(m_accessClk += cycles) - vcpu_tick_base;
        // Adjust base
    while (vcpu_tick_target > vcpu_freq) {
        vcpu_tick_target -= vcpu_freq;
        vcpu_tick_base += vcpu_freq;
        timer_tick_base += PC_TIMER_FREQ;
    }
    
    // We always assume here that vcpu_freq <= PC_TIMER_FREQ
    // timer_ticks_target = timer_ticks_base + vcpu_ticks_target + (u32)(vcpu_ticks_target * V_SCALE >> 32);
    // while ((s32)(timer_ticks - timer_ticks_target) < 0) halt();
    __asm {
        mov eax, [vcpu_tick_target]
        mov ecx, eax
        mul [timer_freq_scale]
        add ecx, timer_tick_base
        add edx, ecx
    again:
        mov eax, [timer_tick]
        sub eax, edx
        jns done
        hlt
        jmp again
    done:
    }
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

    u32 cycles = (u32)m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);
    m_accessClk += cycles;

    //printf("_DEBUG: Innov::read | cycles = %lu\n", cycles);    

    synchronize();
    
    //ioctl(m_handle, HSID_IOCTL_READ, &packet);

    unsigned int packet = inp(INNOV_PORT + addr);

    return (uint8_t)packet;
}

void Innov::write(uint_least8_t addr, uint8_t data)
{
    //printf("_DEBUG: Innov::write call!\n");

    event_clock_t cycles = m_context->getTime(m_accessClk, EVENT_CLOCK_PHI1);
    m_accessClk += cycles;

    //printf("_DEBUG: Innov::write | cycles = %lu\n", cycles);

    synchronize();
    
    outp(INNOV_PORT + addr, data);
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
    for ( unsigned int i = 0; i < voices; i++ )
    {
        cmute |= (muted[i] << i);        
    }

    //ioctl(m_handle, HSID_IOCTL_MUTE, cmute);
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

void Innov::filter(bool enable)
{
   //printf("_DEBUG: Innov::filter call!\n");
       
    //ioctl(m_handle, HSID_IOCTL_NOFILTER, !enable);
}

void Innov::flush()
{
    //printf("_DEBUG: Innov::flush call!\n");
    
    //ioctl(m_handle, HSID_IOCTL_FLUSH);
}

bool Innov::lock(EventContext* env)
{
    //printf("_DEBUG: Innov::lock call!\n");
    
    sidemu::lock(env);
    m_context->schedule(*this, HARDSID_DELAY_CYCLES, EVENT_CLOCK_PHI1);

    // TODO: set 
    vcpu_freq = 1000000; // TODO: read actual frequency <= PC_TIMER_FREQ
    vcpu_tick_base = (u32)m_accessClk;
    timer_tick_base = timer_tick + 2;
    //timer_freq_scale = ((0x100000000ULL * (PC_TIMER_FREQ - vcpu_freq)) / vcpu_freq)
    __asm {
        mov edx, [timer_freq]
        xor eax, eax
        mov ecx, [vcpu_freq]
        sub edx, ecx
        div ecx
        mov [timer_freq_scale], eax
    }
    //timer_freq_scale = ((0x100000000ULL * (PC_TIMER_FREQ - vcpu_freq)) / vcpu_freq)

    printf("_DEBUG: event_frequency: %lu, scale=2**32 + %lu\n", vcpu_freq, timer_freq_scale);
    return true;
}

void Innov::unlock()
{
    //printf("_DEBUG: Innov::unlock call!\n");
    
    m_context->cancel(*this);
    sidemu::unlock();
}

