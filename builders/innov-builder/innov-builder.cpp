/*
 * This file is part of libsidplayfp, a SID player engine.
 *
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

#include "innov.h"

#include <cstring>
#include <string>
#include <memory>
#include <sstream>
#include <algorithm>
#include <new>

#ifdef _WIN32
#  include <iomanip>
#endif

#include "innov-emu.h"


bool InnovBuilder::m_initialised = false;

InnovBuilder::InnovBuilder (const char * const name) :
    sidbuilder (name)
{
    if (!m_initialised)
    {
        if (init() < 0)
            return;
        m_initialised = true;
    }
}

InnovBuilder::~InnovBuilder()
{   // Remove all SID emulations
    remove();
}

// Create a new sid emulation.
unsigned int InnovBuilder::create(unsigned int sids)
{
    m_status = true;

    // Check available devices
    unsigned int count = availDevices();
    if (count == 0)
    {
        m_errorBuffer = "Innovation SSI-2001 ERROR: No devices found";
        goto InnovBuilder_create_error;
    }

    if (count < sids)
        sids = count;

    for (count = 0; count < sids; count++)
    {
        try
        {
            std::auto_ptr<Innov> sid(new Innov(this));

            // SID init failed?
            if (!sid->getStatus())
            {
                m_errorBuffer = sid->error();
                goto InnovBuilder_create_error;
            }
            sidobjs.insert(sid.release());
        }
        // Memory alloc failed?
        catch (std::bad_alloc const &)
        {
            m_errorBuffer.assign(name()).append(" ERROR: Unable to create Innov object");
            goto InnovBuilder_create_error;
        }


    }
    return count;

InnovBuilder_create_error:
    m_status = false;
    return count;
}

unsigned int InnovBuilder::availDevices() const
{
    // Available devices
    return 1;
}

const char *InnovBuilder::credits() const
{
    return Innov::getCredits();
}

void InnovBuilder::flush()
{
    for (emuset_t::iterator it=sidobjs.begin(); it != sidobjs.end(); ++it)
        static_cast<Innov*>(*it)->flush();
}

void InnovBuilder::filter(bool enable)
{
    std::for_each(sidobjs.begin(), sidobjs.end(), applyParameter<Innov, bool>(&Innov::filter, enable));
}

// Load the library and initialise the hardsid
int InnovBuilder::init()
{
    /*HINSTANCE dll;

    if (hsid2.Instance)
        return 0;

    m_status = false;
#ifdef UNICODE
    dll = LoadLibrary(L"HARDSID.DLL");
#else
    dll = LoadLibrary("HARDSID.DLL");
#endif
    if (!dll)
    {
        DWORD err = GetLastError();
        if (err == ERROR_DLL_INIT_FAILED)
            m_errorBuffer = "HARDSID ERROR: hardsid.dll init failed!";
        else
            m_errorBuffer = "HARDSID ERROR: hardsid.dll not found!";
        goto HardSID_init_error;
    }

    {   // Export Needed Version 1 Interface
        HsidDLL1_InitMapper_t mapper;
        mapper = (HsidDLL1_InitMapper_t) GetProcAddress(dll, "InitHardSID_Mapper");

        if (mapper)
            mapper();
        else
        {
            m_errorBuffer = "HARDSID ERROR: hardsid.dll is corrupt!";
            goto HardSID_init_error;
        }
    }

    {   // Check for the Version 2 interface
        HsidDLL2_Version_t version;
        version = (HsidDLL2_Version_t) GetProcAddress(dll, "HardSID_Version");
        if (!version)
        {
            m_errorBuffer = "HARDSID ERROR: hardsid.dll not V2";
            goto HardSID_init_error;
        }
        hsid2.Version = version ();
    }

    {
        WORD version = hsid2.Version;
        if ((version >> 8) != (HSID_VERSION_MIN >> 8))
        {
            std::ostringstream ss;
            ss << "HARDSID ERROR: hardsid.dll not V" << (HSID_VERSION_MIN >> 8) << std::endl;
            m_errorBuffer = ss.str();
            goto HardSID_init_error;
        }

        if (version < HSID_VERSION_MIN)
        {
            std::ostringstream ss;
            ss.fill('0');
            ss << "HARDSID ERROR: hardsid.dll hardsid.dll must be V";
            ss << std::setw(2) << (HSID_VERSION_MIN >> 8);
            ss << ".";
            ss << std::setw(2) << (HSID_VERSION_MIN & 0xff);
            ss <<  " or greater" << std::endl;
            m_errorBuffer = ss.str();
            goto HardSID_init_error;
        }
    }

    // Export Needed Version 2 Interface
    hsid2.Delay    = (HsidDLL2_Delay_t)   GetProcAddress(dll, "HardSID_Delay");
    hsid2.Devices  = (HsidDLL2_Devices_t) GetProcAddress(dll, "HardSID_Devices");
    hsid2.Filter   = (HsidDLL2_Filter_t)  GetProcAddress(dll, "HardSID_Filter");
    hsid2.Flush    = (HsidDLL2_Flush_t)   GetProcAddress(dll, "HardSID_Flush");
    hsid2.MuteAll  = (HsidDLL2_MuteAll_t) GetProcAddress(dll, "HardSID_MuteAll");
    hsid2.Read     = (HsidDLL2_Read_t)    GetProcAddress(dll, "HardSID_Read");
    hsid2.Sync     = (HsidDLL2_Sync_t)    GetProcAddress(dll, "HardSID_Sync");
    hsid2.Write    = (HsidDLL2_Write_t)   GetProcAddress(dll, "HardSID_Write");

    if (hsid2.Version < HSID_VERSION_204)
        hsid2.Reset  = (HsidDLL2_Reset_t)  GetProcAddress(dll, "HardSID_Reset");
    else
    {
        hsid2.Lock   = (HsidDLL2_Lock_t)   GetProcAddress(dll, "HardSID_Lock");
        hsid2.Unlock = (HsidDLL2_Unlock_t) GetProcAddress(dll, "HardSID_Unlock");
        hsid2.Reset2 = (HsidDLL2_Reset2_t) GetProcAddress(dll, "HardSID_Reset2");
    }

    if (hsid2.Version < HSID_VERSION_207)
        hsid2.Mute   = (HsidDLL2_Mute_t)   GetProcAddress(dll, "HardSID_Mute");
    else
        hsid2.Mute2  = (HsidDLL2_Mute2_t)  GetProcAddress(dll, "HardSID_Mute2");

    hsid2.Instance = dll;
    */
    
    m_status       = true;
    return 0;

Innov_init_error:

    return -1;
}
