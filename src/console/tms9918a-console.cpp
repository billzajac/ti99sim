//----------------------------------------------------------------------------
//
// File:        tms9918a-console.cpp
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

#include <ctype.h>
#include <stdio.h>
#include "common.hpp"
#include "tms9918a-console.hpp"
#include "screenio.hpp"

cConsoleTMS9918A::cConsoleTMS9918A ( int refresh ) :
    cTMS9918A ( refresh ),
    m_Bias ( 0 ),
    m_Width ( 32 )
{
}

cConsoleTMS9918A::~cConsoleTMS9918A ()
{
}

void cConsoleTMS9918A::Reset ()
{
    cTMS9918A::Reset ();

    m_Bias  = 0;
    m_Width = 32;

    char clear [] = "        ";
    for ( int y = 0; y < 24; y++ ) {
        PutXY ( 0, y, ( char * ) m_Memory, m_Width );
        if ( m_Width == 32 ) PutXY ( 32, y, clear, 8 );
    }
}

bool cConsoleTMS9918A::SetMode ( int mode )
{
    if ( cTMS9918A::SetMode ( mode ) == false ) return false;

    m_Width = ( m_Mode & VDP_M1 ) ? 40 : 32;

    if ( m_Width == 32 ) {
        char clear [] = "        ";
        for ( int y = 0; y < 24; y++ ) {
            PutXY ( 32, y, clear, 8 );
        }
    }

    // Now force a screen update
    int bias = m_Bias;

    m_Bias = ( UINT8 ) -1;

    SetBias (( UINT8 ) bias );

    return true;
}

void cConsoleTMS9918A::Refresh ( bool force )
{
    if ( force == false ) return;

    for ( unsigned i = ( UINT8 * ) m_ImageTable - m_Memory; i < sizeof ( sScreenImage ); i++ ) {
        if ( i / m_Width < 24 ) {
            UINT8 temp = ( UINT8 ) ( m_Memory [ i ] - m_Bias );
            if ( ! isprint ( temp )) temp = '.';
            PutXY ( i % m_Width, i / m_Width, ( char * ) &temp, 1 );
        }
    }
}

void cConsoleTMS9918A::FlipAddressing ()
{
    cTMS9918A::FlipAddressing ();

    Refresh ( true );
}

void cConsoleTMS9918A::WriteData ( UINT8 data )
{
    UINT8 old = m_Memory [ m_Address & 0x3FFF ];
    if ( data != old ) {
        if ((( m_Address & 0x3FFF ) >= ( UINT8 * ) m_ImageTable - m_Memory ) && (( m_Address & 0x3FFF ) < ( UINT8 * ) ( m_ImageTable + 1 ) - m_Memory )) {
            int imageTableOffset = ( UINT8 * ) m_ImageTable - m_Memory;
            int loc = ( m_Address & 0x3FFF ) - imageTableOffset;
            if ( loc / m_Width < 24 ) {
                UINT8 temp = ( UINT8 ) ( data - m_Bias );
                if ( ! isprint ( temp )) temp = '.';
                PutXY ( loc % m_Width, loc / m_Width, ( char * ) &temp, 1 );
            }
        }
    }

    cTMS9918A::WriteData ( data );
}

void cConsoleTMS9918A::WriteRegister ( size_t reg, UINT8 value )
{
    int x = 48 + ( reg % 4 ) * 9;
    int y = ( reg < 4 ) ? 9 : 10;
    GotoXY ( x, y );    outByte ( value );

    cTMS9918A::WriteRegister ( reg, value );
}

void cConsoleTMS9918A::SetBias ( UINT8 bias )
{
    if ( bias != m_Bias ) {
        m_Bias = bias;
        Refresh ( true );
    }
}
