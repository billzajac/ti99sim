//----------------------------------------------------------------------------
//
// File:        option.cpp
// Date:        16-Jul-2001
// Programmer:  Marc Rousseau
//
// Description: A simple set of option parsing routines (similar to libpopt)
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

// Common global to several applications
int verbose;

void PrintHelp ( int count, const sOption *optList )
{
    FUNCTION_ENTRY ( NULL, "PrintHelp", true );

    PrintUsage ();

    if ( count == 0 ) return;

    fprintf ( stdout, "Options:\n" );

    for ( int i = 0; i < count; i++ ) {
        char buffer [80];
        char *ptr = buffer;
        if ( optList->Short ) {
            ptr += sprintf ( ptr, " -%c", optList->Short );
            if ( optList->Long ) ptr += sprintf ( ptr, "," );
        }
        if ( optList->Long ) {
            sprintf ( ptr, " --%s", optList->Long );
            ptr = strchr ( buffer, '*' );
            if ( ptr != NULL ) strcpy ( ptr, ptr + 1 );
        }
        fprintf ( stdout, "  %-25.25s  %s\n", buffer, optList->Help );

        optList++;
    }

    fprintf ( stdout, "\n" );
}

static sOption *FindOption ( const char *arg, int count, sOption *optList )
{
    if ( *arg != '-' ) {
        for ( int i = 0; i < count; i++ ) {
            if ( optList [i].Short == *arg ) {
                bool ok = ( arg[1] == '\0' ) ? true : false;
                if (( ok == false ) && ( arg[1] == '=' )) {
                    ok = (( optList [i].Func != NULL ) || (( optList [i].Flags & OPT_VALUE_PARSE ) != 0 )) ? true : false;
                }
                if ( ok == false ) {
                    fprintf ( stderr, "Illegal option '-%s'\n", arg );
                    return NULL;
                }
                return &optList [i];
            }
        }
    } else {
        for ( int i = 0; i < count; i++ ) {
            const char *longname = optList [i].Long;
            if ( longname == NULL ) continue;
            const char *ptr = strchr ( longname, '*' );
            size_t max = ( ptr == NULL ) ? strlen ( longname ) : ( size_t ) ( ptr - longname );
            if ( strncmp ( arg + 1, longname, max ) == 0 ) {
                if (( ptr != NULL ) || ( arg [ max + 1 ] == '\0' )) {
                    return &optList [i];
                }
            }
        }
    }

    return NULL;
}

static bool ParseOption ( const char *arg, sOption *opt )
{
    if ( opt->Func != NULL ) {
        if ( opt->Func ( arg + 2, opt->Arg ) == false ) {
            fprintf ( stderr, "Invalid option '%s'\n", arg );
            return false;
        }
        return true;
    }

    const char *ptr = strstr ( arg, "=" );

    switch ( opt->Flags ) {
        case OPT_VALUE_PARSE | OPT_VALUE_SET | OPT_SIZE_INT :
            if ( ptr == NULL ) {
                * ( int * ) opt->Arg = opt->Value;
                break;
            }
        case OPT_VALUE_PARSE :
            if ( ptr == NULL ) {
                fprintf ( stderr, "Expected '=X' after option '%s'\n", arg );
                return false;
            }
            * ( int * ) opt->Arg = atoi ( ptr + 1 );
            break;
        case OPT_VALUE_SET | OPT_SIZE_BOOL :
            * ( bool * ) opt->Arg = * ( bool * ) &opt->Value;
            break;
        case OPT_VALUE_SET | OPT_SIZE_INT :
            * ( int * ) opt->Arg = opt->Value;
            break;
        case OPT_VALUE_OR :
            * ( int * ) opt->Arg |= opt->Value;
            break;
        case OPT_VALUE_AND :
            * ( int * ) opt->Arg &= opt->Value;
            break;
        case OPT_VALUE_NOT :
            * ( int * ) opt->Arg &= ~opt->Value;
            break;
        case OPT_VALUE_XOR :
            * ( int * ) opt->Arg ^= opt->Value;
            break;
    }


    return true;
}

int ParseArgs ( int index, int argc, const char * const argv [], int count, sOption *optList )
{
    FUNCTION_ENTRY ( NULL, "ParseArgs", true );

    sOption optHelp = { 'h', "help", OPT_NONE, 0, 0, NULL, NULL };

    while (( index < argc ) && ( argv [index][0] == '-' )) {
        if ( FindOption ( argv [index] + 1, 1, &optHelp ) != NULL ) {
            PrintHelp ( count, optList );
            exit ( 0 );
        }
        sOption *opt = FindOption ( argv [index] + 1, count, optList );
        if ( opt == NULL ) {
            fprintf ( stderr, "Unrecognized option '%s'\n", argv [index] );
            exit ( -1 );
        }

        if ( ParseOption ( argv [index], opt ) == false ) {
            exit ( -1 );
        }

        index++;
    }

    return index;
}
