//----------------------------------------------------------------------------
//
// File:        say.cpp
// Date:        25-Apr-2003
// Programmer:  Marc Rousseau
//
// Description: Simple test program for the TMS5220 Speech Synthesizer code
//
// Copyright (c) 2003-2004 Marc Rousseau, All Rights Reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA.
//
// Revision History:
//
//----------------------------------------------------------------------------

#if defined ( OS_WINDOWS )
  #include <windows.h>
#endif

#include <cstring>
#include "common.hpp"
#include "logger.hpp"
#include "SDL.h"
#include "tms5220.hpp"
#include "tms9919.hpp"
#include "tms9919-sdl.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

struct sNode {
    char      string [64];
    UINT32    prevAddr;
    UINT32    nextAddr;
    UINT32    dataAddr;
    int       dataLength;
};

bool ReadNode ( cTMS5220 *speech, UINT32 address, sNode *node )
{
    FUNCTION_ENTRY ( NULL, "ReadNode", true );

    memset ( node, 0, sizeof ( sNode ));

    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  0 ) & 0x000F )));  // Load Address
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  4 ) & 0x000F )));  // Load Address
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  8 ) & 0x000F )));  // Load Address
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >> 12 ) & 0x000F )));  // Load Address
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >> 16 ) & 0x000F )));  // Load Address

    speech->WriteData ( 0x10 );  // Read byte
    int length = speech->ReadData ( 0 );
    for ( int i = 0; i < length; i++ ) {
        speech->WriteData ( 0x10 );
        node->string [i] = speech->ReadData ( 0 );
    }
    node->string [length] = '\0';

    speech->WriteData ( 0x10 );
    node->prevAddr = speech->ReadData ( 0 ) << 8;
    speech->WriteData ( 0x10 );
    node->prevAddr |= speech->ReadData ( 0 );

    speech->WriteData ( 0x10 );
    node->nextAddr = speech->ReadData ( 0 ) << 8;
    speech->WriteData ( 0x10 );
    node->nextAddr |= speech->ReadData ( 0 );

    speech->WriteData ( 0x10 );
    speech->ReadData ( 0 );

    speech->WriteData ( 0x10 );
    node->dataAddr = speech->ReadData ( 0 ) << 8;
    speech->WriteData ( 0x10 );
    node->dataAddr |= speech->ReadData ( 0 );

    speech->WriteData ( 0x10 );
    node->dataLength = speech->ReadData ( 0 );

    return true;
}

UINT32 LocateString ( cTMS5220 *speech, const char *string, size_t length )
{
    // Reset the LoadPointer
    speech->WriteData ( 0x40 );  // Load Address
    speech->WriteData ( 0x40 );  // Load Address
    speech->WriteData ( 0x40 );  // Load Address
    speech->WriteData ( 0x40 );  // Load Address
    speech->WriteData ( 0x40 );  // Load Address

    speech->WriteData ( 0x10 );  // Read byte
    if ( speech->ReadData ( 0 ) != 0xAA ) {
        fprintf ( stderr, "Speech rom is invalid!\n" );
        return 0;
    }

    sNode node;
    UINT32 addr = 1;

    for ( EVER ) {
        ReadNode ( speech, addr, &node );
        int delta = strnicmp ( string, node.string, length );
        if (( strlen ( node.string ) == length ) && ( delta == 0 )) break;
        addr = ( delta <= 0 ) ? node.prevAddr : node.nextAddr;
        if ( addr == 0 ) return 0;
    }

    return node.dataAddr;
}

void SayPhrase ( cTMS5220 *speech, const char *text, size_t length )
{
    FUNCTION_ENTRY ( NULL, "SayPhrase", true );

    UINT32 address = LocateString ( speech, text, length );
    if ( verbose > 1 ) {
        fprintf ( stdout, "Phrase: %*.*s Address: %04X\n", ( int ) length, ( int ) length, text, address );
    }

    // If we can't find the phrase in the dictionary, spell it out
    if ( address == 0 ) {
        if ( verbose > 0 ) {
            fprintf ( stdout, "Unable to find phrase '%*.*s'\n", ( int ) length, ( int ) length, text );
        }
        if ( length == 1 ) {
            // Do the 'Uhoh' thing, but make sure we don't get stuck in a recursive loop
            if ( LocateString ( speech, "UHOH", 4 ) != 0 ) {
                SayPhrase ( speech, "UHOH", 4 );
            }
        } else {
            for ( size_t i = 0; i < length; i++ ) {
                SayPhrase ( speech, text + i, 1 );
            }
        }
        return;
    }

    // Load the address of the speech data
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  0 ) & 0x000F )));
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  4 ) & 0x000F )));
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >>  8 ) & 0x000F )));
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >> 12 ) & 0x000F )));
    speech->WriteData (( UINT8 ) ( 0x40 | (( address >> 16 ) & 0x000F )));

    // Speek
    speech->WriteData ( 0x50 );

    // Wait for the phrase to complete
    UINT8 status = 0;
    do {
        SDL_Delay ( 200 );
        status = speech->ReadData ( 0 );
    } while ( status == 0xE0 );
}

void Say ( cTMS5220 *speech, const char *text )
{
    FUNCTION_ENTRY ( NULL, "Say", true );

    SayPhrase ( speech, text, strlen ( text ));
}

bool ParseSampleRate ( const char *arg, void *ptr )
{
    FUNCTION_ENTRY ( NULL, "ParseSampleRate", true );

    int freq = 0;

    arg = strchr ( arg, '=' ) + 1;

    if ( sscanf ( arg, "%d", &freq ) != 1 ) {
        fprintf ( stderr, "Invalid sampling rate '%s'\n", arg );
        return false;
    }

    if (( freq > 44100 ) || ( freq < 8000 )) {
        fprintf ( stderr, "Sampling rate must be between 8000 and 44100\n" );
        return false;
    }

    * ( int * ) ptr = freq;

    return true;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: say [options] <Text-to-speak>\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    int  samplingRate   = 44100;
    int  volume         = 50;

    sOption optList [] = {
        { 's', "sample=*<freq>",     OPT_NONE,                      0,     &samplingRate,   ParseSampleRate, "Select sampling frequency for audio playback" },
        { 'v', "verbose*=n",         OPT_VALUE_PARSE_INT,           1,     &verbose,        NULL,            "Display extra information" },
        {  0,  "volume=*n",          OPT_VALUE_PARSE_INT,           50,    &volume,         NULL,            "Set the audio volume" }
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    int index = ParseArgs ( 1, argc, argv, SIZE ( optList ), optList );

    if ( SDL_Init ( SDL_INIT_NOPARACHUTE ) < 0 ) {
        fprintf ( stderr, "Couldn't initialize SDL: %s\n", SDL_GetError ());
        return -1;
    }

    // Clean up on exit, exit on window close and interrupt
    atexit ( SDL_Quit );

    cSdlTMS9919 sound ( samplingRate );
    sound.SetMasterVolume ( volume );

    cTMS5220 speech ( &sound );

    speech.WriteData ( 0x70 );  // Reset

    for ( int i = index; i < argc; i++ ) {
        Say ( &speech, argv [i] );
    }

    return 0;
}
