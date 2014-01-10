//----------------------------------------------------------------------------
//
// File:        option.hpp
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

#ifndef OPTION_HPP_
#define OPTION_HPP_

typedef bool (*optFunc) ( const char *, void * );

#define OPT_NONE            0x00000000
#define OPT_VALUE_PARSE     0x00000001
#define OPT_VALUE_SET       0x00000002
#define OPT_VALUE_OR        0x00000003
#define OPT_VALUE_AND       0x00000004
#define OPT_VALUE_NOT       0x00000005
#define OPT_VALUE_XOR       0x00000006

#define OPT_SIZE_BOOL       0x00000100
#define OPT_SIZE_INT        0x00000200

#define OPT_VALUE_PARSE_INT ( OPT_VALUE_PARSE | OPT_VALUE_SET | OPT_SIZE_INT )

struct sOption {
    char        Short;
    const char *Long;
    int         Flags;
    int         Value;
    void       *Arg;
    optFunc     Func;
    const char *Help;
};

// Common global to several applications
extern int verbose;

// This must be provided by the application
extern void PrintUsage ();

void PrintHelp ( int count, const sOption *optList );
int ParseArgs ( int index, int argc, const char * const argv [], int count, sOption *optList );

#endif
