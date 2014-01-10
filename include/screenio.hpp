//----------------------------------------------------------------------------
//
// File:        screenio.hpp
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

#ifndef SCREENIO_HPP_
#define SCREENIO_HPP_

#if defined ( OS_OS2 ) || defined ( OS_WINDOWS )

    #include <conio.h>

    #if defined ( __GNUC__ )
        #define cprintf _cprintf
    #endif

#elif defined ( __GNUC__ )

    #define stricmp strcasecmp

    extern int cprintf ( const char *, ... );

#endif

#define COLOR_BLACK     0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_YELLO     3
#define COLOR_BLUE      4
#define COLOR_MAGENTA   5
#define COLOR_CYAN      6
#define COLOR_WHITE     7
#define COLOR_DEFAULT  -1

void SaveConsoleSettings ();
void RestoreConsoleSettings ();

void HideCursor ();
void ShowCursor ();

UINT32 GetKey ();
bool   KeyPressed ();

UINT32 CurrentTime ();

void ClearScreen ();
void SetColor ( bool, int, int );

void GetXY ( size_t *x, size_t *y );
void GotoXY ( size_t x, size_t y );
void PutXY ( size_t x, size_t y, const char *ptr, size_t length );
void Put ( const char *ptr, size_t length );

void outByte ( UINT8 val );
void outWord ( UINT16 val );
void outLong ( UINT32 val );

#endif
