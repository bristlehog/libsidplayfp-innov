/*
 * This file is part of libsidplayfp, a SID player engine.
 *
 * Copyright 2011-2014 Leandro Nini <drfiemost@users.sourceforge.net>
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

#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <stdlib.h>
#include <conio.h>

#include "sidplayfp\sidplayfp.h"
#include "sidplayfp\SidTune.h"
#include "sidplayfp\sidbuilder.h"
#include "..\builders\innov-builder\innov.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/*
 * Adjust these paths to point to existing ROM dumps
 */
#define KERNAL_PATH "kernal.bin"
#define BASIC_PATH "basic.bin"
#define CHARGEN_PATH "chargen.bin"

//#define PC64_TESTSUITE "/bin/"

void loadRom(const char* path, char* buffer)
{
    std::ifstream is(path, std::ios::binary);
    if (!is.is_open())
    {
        std::cout << "File " << path << " not found" << std::endl;
        exit(-1);
    }
    is.read(buffer, 8192);
    is.close();
}

int main(int argc, char* argv[])
{
    sidplayfp m_engine;

    char kernal[8192];
    char basic[8192];
    char chargen[4096];

    std::vector<short> buffer(8000);

    loadRom(KERNAL_PATH, kernal);
    loadRom(BASIC_PATH, basic);
    loadRom(CHARGEN_PATH, chargen);

    m_engine.setRoms((const uint8_t*)kernal, (const uint8_t*)basic, (const uint8_t*)chargen);


/*
    // Get the number of SIDs supported by the engine
    unsigned int maxsids = ( m_engine.info() ).maxsids();

    printf("\nMaximum number of SIDs supported by the engine: %u\n", maxsids);
    
*/
    std::string name("");

    if(argc > 1)
    {
        name.append(argv[1]);
    }

    /*if (argc > 1)
    {
        name.append(argv[1]).append(".prg");
    }
    else
    {
        name.append("start.prg");
    }*/

    //printf("\nTune name: %s\n\n", name.c_str());

    //printf("Playing %s, press any key to exit...", name.c_str());

    // Set up a SID builder
    std::auto_ptr<InnovBuilder> inn(new InnovBuilder("Innovation SSI-2001 builder"));

    //printf("_DEBUG: SID builder class has been created.\n");

    // Get the number of SIDs supported by the engine
    unsigned int maxsids = 1; //(m_engine.info()).maxsids();

    // Create SID emulators
    inn->create(maxsids);

    // Check if builder is ok
    if (!inn->getStatus())
    {
        std::cerr << inn->error() << std::endl;
        return -1;
    }

    //printf("_DEBUG: SID builder has been started. MAXSIDS = %u\n", maxsids);

    std::auto_ptr<SidTune> tune(new SidTune(name.c_str()));

    if (!tune->getStatus())
    {
        std::cerr << "Error: " << tune->statusString() << std::endl;
        return -1;
    }

 

    tune->selectSong(0);

   // Configure the engine
    SidConfig cfg;
    cfg.frequency = 44100;
    cfg.samplingMethod = SidConfig::INTERPOLATE;
    cfg.fastSampling = false;
    cfg.playback = SidConfig::MONO;
    cfg.sidEmulation = inn.get();
    
    if (!m_engine.config(cfg))
    {
        std::cerr <<  m_engine.error() << std::endl;
        return -1;
    }
    
    //printf("_DEBUG: engine has been configured.\n");

    if (!m_engine.load(tune.get()))
    {
        std::cerr <<  m_engine.error() << std::endl;
        return -1;
    }

    



    m_engine.play(0, 0);

    for (;;)
    {
        if(!m_engine.isPlaying () || kbhit())
        {
            break;
        }
    }

    // flush keyboard buffer
    while(kbhit())
    {
        getch();
    }
    //printf("\n\n");
}
