//----------------------------------------------------------------------------
//
// File:        ti-main.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 1998-2004 Marc Rousseau, All Rights Reserved.
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
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "cartridge.hpp"
#include "ti994a-console.hpp"
#include "tms9918a-console.hpp"
#include "ti-disk.hpp"
#include "screenio.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

static char *diskImage [3];

bool ParseDisk ( const char *arg, void * )
{
    FUNCTION_ENTRY ( NULL, "ParseDisk", true );

    arg += strlen ( "dsk" );

    int disk = arg [0] - '1';
    if (( disk < 0 ) || ( disk > 2 ) || ( arg [1] != '=' )) {
        return false;
    }

    diskImage [disk] = strdup ( arg + 2 );

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

    fprintf ( stdout, "Usage: ti99sim-console [options] [cartridge] [image]\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    int refreshRate = 60;

    sOption optList [] = {
        {  0,  "dsk*n=<filename>", OPT_NONE,                      0,     NULL,            ParseDisk,      "Use <filename> disk image for DSKn" },
        {  0,  "NTSC",             OPT_VALUE_SET | OPT_SIZE_INT,  60,    &refreshRate,    NULL,           "Emulate a NTSC display (60Hz)" },
        {  0,  "PAL",              OPT_VALUE_SET | OPT_SIZE_INT,  50,    &refreshRate,    NULL,           "Emulate a PAL display (50Hz)" },
        { 'v', "verbose*=n",       OPT_VALUE_PARSE_INT,           1,     &verbose,        NULL,           "Display extra information" },
    };

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    SaveConsoleSettings ();

    HideCursor ();
    ClearScreen ();

    cTMS9918A *vdp = new cConsoleTMS9918A ( refreshRate );

    const char *romFile = LocateFile ( "TI-994A.ctg", "roms" );
    if ( romFile == NULL ) {
        fprintf ( stderr, "Unable to locate console ROMs!\n" );
        delete vdp;
        return -1;
    }

    if ( verbose > 0 ) fprintf ( stdout, "Using system ROM \"%s\"\n", romFile );
    cCartridge *consoleROM = new cCartridge ( romFile );

    cConsoleTI994A computer ( consoleROM, vdp );

    cDiskDevice *disk = new cDiskDevice ( LocateFile ( "ti-disk.ctg", "roms" ));
    for ( unsigned i = 0; i < SIZE ( diskImage ); i++ ) {
        char dskName [10];
        sprintf ( dskName, "dsk%d.dsk", i + 1 );
        const char *validName = LocateFile ( diskImage [i], "disks" );
        if ( validName == NULL ) {
            validName = LocateFile ( dskName, "disks" );
            if ( validName == NULL ) {
                validName = dskName;
            }
        }
        disk->LoadDisk ( i, validName );
    }
    computer.AddDevice ( disk );

    cCartridge *ctg = NULL;
    while ( index < argc ) {
        if ( IsType ( argv [index], ".ctg" )) {
            if ( ctg != NULL ) {
                computer.RemoveCartridge ( ctg );
                delete ctg;
            }
            ctg = new cCartridge ( LocateFile ( argv [index], "cartridges" ));
            computer.InsertCartridge ( ctg );
        }

        if ( IsType ( argv [index], ".img" )) {
            (( cTI994A & ) computer).LoadImage ( argv [index] );
        }

        index++;
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

    ClearScreen ();
    ShowCursor ();

    RestoreConsoleSettings ();

    return 0;
}
