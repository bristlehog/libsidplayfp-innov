/*
* This file is part of libsidplayfp, a SID player engine.
*
* Copyright 2015 Boris Chuprin <ej@tut.by>
* Copyright 2014-2015 Pavel Ageev <pageev@mail.ru>
* Copyright 2011-2014 Leandro Nini <drfiemost@users.sourceforge.net>
* Copyright 2007-2010 Antti Lankila
* Copyright 2001-2002 by Jarno Paananen
* Copyright 2000-2002 Simon White
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

#ifndef INNOV_EMU_H
#define INNOV_EMU_H

#include "../../sidplayfp/event.h"
#include "../../sidplayfp/sidemu.h"
#include "../../sidplayfp/EventScheduler.h"
#include "../../sidplayfp/siddefs.h"
#include "innov.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define INNOV_PORT              0x2A0

class sidbuilder;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32

#define INNOV_VOICES 3

// Approx 60ms TODO: Necessary?
#define HARDSID_DELAY_CYCLES 60000

/***************************************************************************
 * Innov SID Specialisation
 ***************************************************************************/
class Innov : public sidemu, private Event
{
private:
    friend class InnovBuilder;

    static const unsigned int voices;
    static       unsigned int sid;

    // Must stay in this order
    bool           muted[INNOV_VOICES];

public:
    static const char* getCredits() {
        return "Innovation SSI-2001 Engine v. 1.1\n";
    }
    
    Innov(sidbuilder *builder);
    ~Innov();

    // Standard component functions
    void reset() { sidemu::reset (); }
    void reset(uint8_t volume);

    uint8_t read(uint_least8_t addr);
    void write(uint_least8_t addr, uint8_t data);

    void clock();
    bool getStatus() const { return m_status; }

    // Standard SID functions
    void filter(bool enable);
    void model(SidConfig::sid_model_t model SID_UNUSED) {;}
    void voice(unsigned int num, bool mute);
    void sampling(float systemfreq, float outputfreq,
        SidConfig::sampling_method_t method, bool fast);

    // Must lock the SID before using the standard functions.
    bool lock(EventContext *env);
    
    void unlock();

private:
    // Fixed interval timer delay to prevent sidplay2
    // shoot to 100% CPU usage when song nolonger
    // writes to SID.
    void event();

    // Innov specific

    /// Realtime wait until target time
    void wait();
    /// Update m_accessClk and wait()
    void synchronize();
    /// Reset internal timers
    bool reset_timer();

    static u8 in(unsigned port);
    static void out(unsigned port, u8 data);
};


// Platform-dependent timer initialization/deinitialization code. Both called at most once per session
void timer_create(void);
void timer_free(void);

extern u32 vcpu_freq;

#endif // INNOV_EMU_H
