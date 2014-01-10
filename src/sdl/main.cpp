//----------------------------------------------------------------------------
//
// File:        main.cpp
// Date:        02-Apr-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains starup code for Linux/SDL
//
// Copyright (c) 2000-2004 Marc Rousseau, All Rights Reserved.
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

#include <stdio.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "SDL.h"
#include "tms9900.hpp"
#include "ti994a.hpp"
#include "ti994a-sdl.hpp"
#include "tms9918a.hpp"
#include "tms9918a-sdl.hpp"
#include "tms9919.hpp"
#include "tms9919-sdl.hpp"
#include "tms5220.hpp"
#include "device.hpp"
#include "diskio.hpp"
#include "ti-disk.hpp"
#include "support.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

static sRGBQUAD ColorTable [3][17] = {
    {
        { 0x00, 0x00, 0x00, 0x00 },		// 0x00 TI_TRANSPARENT - Background
        { 0x00, 0x00, 0x00, 0x00 },		// 0x01 TI_BLACK
        { 0x48, 0x9C, 0x08, 0x00 },		// 0x02 TI_MEDIUM_GREEN *
        { 0x70, 0xBF, 0x88, 0x00 },		// 0x03 TI_LIGHT_GREEN *
        { 0x28, 0x3C, 0x8A, 0x00 },		// 0x04 TI_DARK_BLUE *
        { 0x50, 0x6C, 0xCF, 0x00 },		// 0x05 TI_LIGHT_BLUE *
        { 0xD0, 0x48, 0x00, 0x00 },		// 0x06 TI_DARK_RED *
        { 0x00, 0xCC, 0xFF, 0x00 },		// 0x07 TI_CYAN *
        { 0xD0, 0x58, 0x28, 0x00 },		// 0x08 TI_MEDIUM_RED *
        { 0xFF, 0xA0, 0x40, 0x00 },		// 0x09 TI_LIGHT_RED *
        { 0xFC, 0xF0, 0x50, 0x00 },		// 0x0A TI_DARK_YELLOW *
        { 0xFF, 0xFF, 0x80, 0x00 },		// 0x0B TI_LIGHT_YELLOW *
        { 0x00, 0x80, 0x00, 0x00 },		// 0x0C TI_DARK_GREEN
        { 0xCD, 0x58, 0xCD, 0x00 },		// 0x0D TI_MAGENTA *
        { 0xE0, 0xE0, 0xE0, 0x00 },		// 0x0E TI_GRAY *
        { 0xFF, 0xFF, 0xFF, 0x00 },		// 0x0F TI_WHITE
        { 0xFF, 0xFF, 0xFF, 0x00 }		// 0x10 - Text Mode Foreground
    }, {
        { 0x00, 0x00, 0x00, 0x00 },		// 0x00 TI_TRANSPARENT - Background
        { 0x00, 0x00, 0x00, 0x00 },		// 0x01 TI_BLACK
        {  33, 200,  66, 0x00 },		// 0x02 TI_MEDIUM_GREEN *
        {  94, 220, 120, 0x00 },		// 0x03 TI_LIGHT_GREEN *
        {  84,  85, 237, 0x00 },		// 0x04 TI_DARK_BLUE *
        { 125, 118, 252, 0x00 },		// 0x05 TI_LIGHT_BLUE *
        { 212,  82,  77, 0x00 },		// 0x06 TI_DARK_RED *
        {  66, 235, 245, 0x00 },		// 0x07 TI_CYAN *
        { 252,  85,  84, 0x00 },		// 0x08 TI_MEDIUM_RED *
        { 255, 121, 120, 0x00 },		// 0x09 TI_LIGHT_RED *
        { 212, 193,  84, 0x00 },		// 0x0A TI_DARK_YELLOW *
        { 230, 206, 128, 0x00 },		// 0x0B TI_LIGHT_YELLOW *
        {  33, 176,  59, 0x00 },		// 0x0C TI_DARK_GREEN
        { 201,  91, 186, 0x00 },		// 0x0D TI_MAGENTA *
        { 204, 204, 204, 0x00 },		// 0x0E TI_GRAY *
        { 0xFF, 0xFF, 0xFF, 0x00 },		// 0x0F TI_WHITE
        { 0xFF, 0xFF, 0xFF, 0x00 }		// 0x10 - Text Mode Foreground
    }, {
        { 0x00, 0x00, 0x00, 0x00 },		// 0x00 TI_TRANSPARENT - Background
        { 0x00, 0x00, 0x00, 0x00 },		// 0x01 TI_BLACK
        { 0x00, 0xCC, 0x00, 0x00 },		// 0x02 TI_MEDIUM_GREEN
        { 0x00, 0xFF, 0x00, 0x00 },		// 0x03 TI_LIGHT_GREEN
        { 0x00, 0x00, 0x80, 0x00 },		// 0x04 TI_DARK_BLUE
        { 0x00, 0x00, 0xFF, 0x00 },		// 0x05 TI_LIGHT_BLUE
        { 0x80, 0x00, 0x00, 0x00 },		// 0x06 TI_DARK_RED
        { 0x00, 0xFF, 0xFF, 0x00 },		// 0x07 TI_CYAN
        { 0xCC, 0x00, 0x00, 0x00 },		// 0x08 TI_MEDIUM_RED
        { 0xFF, 0x00, 0x00, 0x00 },		// 0x09 TI_LIGHT_RED
        { 0xB0, 0xB0, 0x00, 0x00 },		// 0x0A TI_DARK_YELLOW
        { 0xFF, 0xFF, 0x00, 0x00 },		// 0x0B TI_LIGHT_YELLOW
        { 0x00, 0x80, 0x00, 0x00 },		// 0x0C TI_DARK_GREEN
        { 0xB0, 0x00, 0xB0, 0x00 },		// 0x0D TI_MAGENTA
        { 0xCC, 0xCC, 0xCC, 0x00 },		// 0x0E TI_GRAY
        { 0xFF, 0xFF, 0xFF, 0x00 },		// 0x0F TI_WHITE
        { 0xFF, 0xFF, 0xFF, 0x00 }		// 0x10 - Text Mode Foreground
    }
};

static int   geometryX            = VDP_WIDTH;
static int   geometryY            = VDP_HEIGHT;
static bool  joystickInitialized  = false;
static int   joystickIndex [2]    = { 0, 1 };
static int   framesOn             = 1;
static int   framesOff            = 0;
static char *diskImage [3];

bool ListJoysticks ( const char *, void * )
{
    FUNCTION_ENTRY ( NULL, "ListJoysticks", true );

    if (( joystickInitialized == false ) && ( SDL_InitSubSystem ( SDL_INIT_JOYSTICK ) < 0 )) {
        fprintf ( stderr, "Error initializing joystick subsystem: %s\n", SDL_GetError ());
        return false;
    }

    joystickInitialized = true;

    int num = SDL_NumJoysticks ();
    fprintf ( stdout, ( num == 1 ) ? "1 joystick was found.\n" : "%i joysticks were found.\n", num );

    if ( num > 0 ) fprintf ( stdout, "\nThe names of the joysticks are:\n" );
    for ( int i = 0; i < SDL_NumJoysticks (); i++ ) {
        fprintf ( stdout, "  %d) %s\n", i + 1, SDL_JoystickName ( i ));
    }

    exit ( 0 );
}

bool ListResolutions ( const char *, void * )
{
    FUNCTION_ENTRY ( NULL, "ListResolutions", true );

    const int MAX_RESOLUTIONS = 32;

    int xDisplay [MAX_RESOLUTIONS];
    int yDisplay [MAX_RESOLUTIONS];

    int n = cSdlTMS9918A::GetFullScreenResolutions ( xDisplay, yDisplay, MAX_RESOLUTIONS );

    fprintf ( stdout, "Available full screen resolutions:\n" );

    for ( int i = 0; i < n; i++ ) {
        fprintf ( stdout, "%2d - %4dx%d\n", i + 1, xDisplay [i], yDisplay [i] );
    }

    exit ( 0 );
}

bool ParseJoystick ( const char *arg, void * )
{
    FUNCTION_ENTRY ( NULL, "ParseJoystick", true );

    arg += strlen ( "joystick" );

    int joy_num = arg [0] - '1';
    if (( joy_num < 0 ) || ( joy_num > 1 ) || ( arg [1] != '=' )) {
        fprintf ( stderr, "Joystick must be either 1 or 2\n" );
        return false;
    }

    if (( joystickInitialized == false ) && ( SDL_InitSubSystem ( SDL_INIT_JOYSTICK ) < 0 )) {
        fprintf ( stderr, "Error initializing joystick subsystem: %s\n", SDL_GetError ());
        return false;
    }

    joystickInitialized = true;

    int joy_index = atoi ( arg + 2 ) - 1;
    if (( joy_index < 0 ) || ( joy_index >= SDL_NumJoysticks ())) {
        if ( SDL_NumJoysticks () == 0 ) {
            fprintf ( stderr, "No joysticks were detected\n" );
        } else {
            fprintf ( stderr, "Joystick %s is invalid (range=%d-%d)\n", arg + 2, 1, SDL_NumJoysticks ());
        }
        return false;
    }
    joystickIndex [joy_num] = joy_index;

    return true;
}

bool ParseDisk ( const char *arg, void * )
{
    FUNCTION_ENTRY ( NULL, "ParseDisk", true );

    arg += strlen ( "dsk" );

    int disk = arg [0] - '1';
    if (( disk < 0 ) || ( disk > 2 ) || ( arg [1] != '=' )) {
        fprintf ( stderr, "Disk must be either 1, 2, or 3\n" );
        return false;
    }

    diskImage [disk] = strdup ( arg + 2 );

    return true;
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

bool ParseFrameRate ( const char *arg, void * )
{
    FUNCTION_ENTRY ( NULL, "ParseFrameRate", true );

    int num = 0, den = 0;

    arg = strchr ( arg, '=' ) + 1;

    switch ( sscanf ( arg, "%d/%d", &num, &den )) {
        case 1 :
            den = 100;
            break;
        case 2 :
            break;
        default :
            fprintf ( stderr, "Invalid framerate '%s'\n", arg );
            return false;
    }

    if (( num > den ) || ( num * den == 0 )) {
        fprintf ( stderr, "Invalid framerate specified\n" );
        return false;
    }

    framesOn  = num;
    framesOff = den - num;

    return true;
}

bool IsType ( const char *filename, const char *type )
{
    FUNCTION_ENTRY ( NULL, "IsType", true );

    size_t len = strlen ( filename );
    const char *ptr = filename + len - 4;
    return ( strcmp ( ptr, type ) == 0 ) ? true : false;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: ti99sim-sdl [options] [cartridge.ctg] [image.img]\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv [] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    int  flagSize        = 1;
    bool flagSound       = true;
    bool flagSpeech      = true;
    bool flagJoystick    = true;
    int  colorTableIndex = 0;
    int  fullScreenMode  = -1;
    int  refreshRate     = 60;
    int  samplingRate    = 44100;
    bool useScale2x      = false;
    int  volume          = 50;

    sOption optList [] = {
        { '4', NULL,                 OPT_VALUE_SET | OPT_SIZE_INT,  2,     &flagSize,        NULL,            "Double width/height window" },
        {  0,  "dsk*n=<filename>",   OPT_NONE,                      0,     NULL,             ParseDisk,       "Use <filename> disk image for DSKn" },
        {  0,  "framerate=*{n/d|p}", OPT_NONE,                      0,     NULL,             ParseFrameRate,  "Reduce frame rate to fraction n/d or percentage p" },
        { 'f', "fullscreen*=n",      OPT_VALUE_PARSE_INT,           0,     &fullScreenMode,  NULL,            "Fullscreen" },
        {  0,  "joystick*n=i",       OPT_NONE,                      0,     NULL,             ParseJoystick,   "Use system joystick i as TI joystick n" },
        {  0,  "list-joysticks",     OPT_NONE,                      0,     NULL,             ListJoysticks,   "Print a list of all detected joysticks" },
        {  0,  "list-resolutions",   OPT_NONE,                      0,     NULL,             ListResolutions, "Print a list of available fullscreen resolutions" },
        {  0,  "no-joystick",        OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagJoystick,    NULL,            "Disable hardware joystick support" },
        { 'q', "no-sound",           OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagSound,       NULL,            "Turn off all sound/speech" },
        {  0,  "no-speech",          OPT_VALUE_SET | OPT_SIZE_BOOL, false, &flagSpeech,      NULL,            "Disable speech synthesis" },
        {  0,  "NTSC",               OPT_VALUE_SET | OPT_SIZE_INT,  60,    &refreshRate,     NULL,            "Emulate a NTSC display (60Hz)" },
        {  0,  "PAL",                OPT_VALUE_SET | OPT_SIZE_INT,  50,    &refreshRate,     NULL,            "Emulate a PAL display (50Hz)" },
        { 'p', "palette=*n",         OPT_VALUE_PARSE_INT,           0,     &colorTableIndex, NULL,            "Select a color palette (1-3)" },
        { 's', "sample=*<freq>",     OPT_NONE,                      0,     &samplingRate,    ParseSampleRate, "Select sampling frequency for audio playback" },
        {  0,  "scale2x",            OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &useScale2x,      NULL,            "Use the Scale2x algorithm to scale display" },
        { 'v', "verbose*=n",         OPT_VALUE_PARSE_INT,           1,     &verbose,         NULL,            "Display extra information" },
        {  0,  "volume=*n",          OPT_VALUE_PARSE_INT,           50,    &volume,          NULL,            "Set the audio volume" }
    };

    // Initialize the SDL library (starts the event loop)
    if ( SDL_Init ( SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 ) {
        fprintf ( stderr, "Couldn't initialize SDL: %s\n", SDL_GetError ());
        return -1;
    }

    // Clean up on exit, exit on window close and interrupt
    atexit ( SDL_Quit );

    SDL_WM_SetCaption ( "TI-99/sim", NULL );

    SDL_EnableUNICODE ( 1 );

    const char *ctgFile = NULL;
    const char *imgFile = NULL;

    int index = 1;

    bool isValid = true;

    while ( index < argc ) {

        int newIndex = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

        if ( index == newIndex ) {

            if ( IsType ( argv [index], ".ctg" )) {

                const char *filename = LocateFile ( argv [index], "cartridges" );
                if ( filename != NULL ) {
                    if ( ctgFile != NULL ) free (( void * ) ctgFile );
                    ctgFile = strdup ( filename );
                } else {
                    isValid = false;
                    fprintf ( stderr, "Unable to locate cartridge \"%s\"\n", argv [index] );
                }

            } else if ( IsType ( argv [index], ".img" )) {

                const char *filename = LocateFile ( argv [index] );
                if ( filename != NULL ) {
                    isValid = true;
                    if ( imgFile != NULL ) free (( void * ) imgFile );
                    imgFile = strdup ( filename );
                } else {
                    isValid = false;
                    fprintf ( stderr, "Unable to locate image \"%s\"\n", argv [index] );
                }

            } else {

                isValid = false;
                fprintf ( stderr, "Unrecognized argument \"%s\"\n", argv [index] );

            }


            index++;

        } else {

            index = newIndex;

        }
    }

    if ( isValid == false ) {
        return 0;
    }

    if ( colorTableIndex > 0 ) {
        if ( colorTableIndex > 3 ) {
            fprintf ( stderr, "Invalid palette selected - must be 1 or 2\n" );
            return -1;
        }
        colorTableIndex--;
    }

    SDL_Joystick *joy1 = NULL;
    SDL_Joystick *joy2 = NULL;

    if ( flagJoystick == true ) {
        // Try to initialize the joystick subsystem and allocate them
        if (( joystickInitialized == true ) || ( SDL_InitSubSystem ( SDL_INIT_JOYSTICK ) >= 0 )) {
            // NOTE: If we called SDL_InitSubSystem here, the user didn't explicitly request joystick support
            //   and we'll silently ignore the failure and disable joysticks (MAC OS X seems to have a
            //   problem with joysticks).  If they asked for them, and it failed it would have been reported
            //   to them in ParseJoystick.
            joystickInitialized = true;
            joy1 = SDL_JoystickOpen ( joystickIndex [0] );
            joy2 = SDL_JoystickOpen ( joystickIndex [1] );
        }
    }

    const char *romFile = LocateFile ( "TI-994A.ctg", "roms" );
    if ( romFile == NULL ) {
        fprintf ( stderr, "Unable to locate console ROMs!\n" );
        return -1;
    }

    if ( verbose > 0 ) fprintf ( stdout, "Using system ROM \"%s\"\n", romFile );
    cCartridge *consoleROM = new cCartridge ( romFile );

    if ( flagSize == 2 ) {
        geometryX *= 2;
        geometryY *= 2;
    }

    if ( fullScreenMode != -1 ) {
        const int MAX_RESOLUTIONS = 32;
        int xDisplay [MAX_RESOLUTIONS];
        int yDisplay [MAX_RESOLUTIONS];
        int n = cSdlTMS9918A::GetFullScreenResolutions ( xDisplay, yDisplay, MAX_RESOLUTIONS );
        if (( fullScreenMode > 0 ) && ( fullScreenMode <= n )) {
            geometryX = xDisplay [fullScreenMode-1];
            geometryY = yDisplay [fullScreenMode-1];
        } else {
            geometryX = 0;
            geometryY = 0;
        }
    }

    if ( flagSound == ( int ) false ) flagSpeech = false;

    cSdlTMS9918A *vdp = new cSdlTMS9918A ( ColorTable [colorTableIndex], refreshRate, useScale2x, ( fullScreenMode != -1 ) ? true : false, geometryX, geometryY );
    cTMS9919 *sound = NULL;
    if ( flagSound == false ) {
        sound = new cTMS9919 ();
    } else {
        cSdlTMS9919 *sdlSound = new cSdlTMS9919 ( samplingRate );
        sdlSound->SetMasterVolume ( volume );
        sound = sdlSound;
    }
    cTMS5220 *speech = ( flagSpeech == false ) ? NULL : new cTMS5220 ( sound );

    vdp->SetFrameRate ( framesOn, framesOff );

    cSdlTI994A computer ( consoleROM, vdp, sound, speech );

    const char *diskFile = LocateFile ( "ti-disk.ctg", "roms" );
    if ( diskFile != NULL ) {
        if ( verbose > 0 ) fprintf ( stdout, "Loading disk ROM \"%s\"\n", diskFile );
        cDiskDevice *disk = new cDiskDevice ( diskFile );
        // Look for DSKn images
        for ( unsigned i = 0; i < SIZE ( diskImage ); i++ ) {
            char buffer [256];
            const char *validName = LocateFile ( diskImage [i], "disks" );
            if ( validName == NULL ) {
                char dskName [10];
                sprintf ( dskName, "dsk%d.dsk", i + 1 );
                validName = LocateFile ( dskName, "disks" );
                if ( validName == NULL ) {
                    // Can't find one - use ~/.ti99sim/disks/dskn.dsk as the default
                    sprintf ( buffer, "%s%c%s%c%s", HOME_PATH, FILE_SEPERATOR, "disks", FILE_SEPERATOR, dskName );
                    validName = buffer;
                }
            }
            if ( verbose > 0 ) fprintf ( stdout, "  DSK%d: \"%s\"\n", i+1, validName );
            disk->LoadDisk ( i, validName );
        }
        computer.AddDevice ( disk );
    } else {
        if ( verbose > 0 ) fprintf ( stdout, "Unable to locate disk ROM images \"%s\"\n", "ti-disk.ctg" );
    }

    if ( joy1 != NULL ) computer.SetJoystick ( 0, joy1 );
    if ( joy2 != NULL ) computer.SetJoystick ( 1, joy2 );

    cCartridge *ctg = NULL;
    if ( ctgFile != NULL ) {
        ctg = new cCartridge ( ctgFile );
        if ( verbose > 0 ) fprintf ( stdout, "Loading cartridge \"%s\" (%s)\n", ctgFile, ctg->Title ());
        computer.InsertCartridge ( ctg );
    }

    if ( imgFile != NULL ) {
        if ( verbose > 0 ) fprintf ( stdout, "Loading image \"%s\"\n", imgFile );
        (( cTI994A & ) computer).LoadImage ( imgFile );
    }

    if ( verbose > 0 ) {
        fprintf ( stdout, " Video refresh rate: %d Hz\n", refreshRate );
        if ( flagSound == true ) fprintf ( stdout, "Audio sampling rate: %d Hz\n", samplingRate );
    }

    computer.Run ();

    if ( ctg != NULL ) {
        computer.RemoveCartridge ( ctg );
        delete ctg;
    }

    for ( unsigned i = 0; i < SIZE ( diskImage ); i++ ) {
        if ( diskImage [i] != NULL ) {
            free ( diskImage [i] );
        }
    }

    if ( SDL_JoystickOpened ( joystickIndex [0] )) SDL_JoystickClose ( joy1 );
    if ( SDL_JoystickOpened ( joystickIndex [1] )) SDL_JoystickClose ( joy2 );

    if ( ctgFile != NULL ) free (( void * ) ctgFile );
    if ( imgFile != NULL ) free (( void * ) imgFile );

    return 0;
}
