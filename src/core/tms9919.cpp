//----------------------------------------------------------------------------
//
// File:        tms9919.cpp
// Date:        20-Mar-1998
// Programmer:  Marc Rousseau
//
// Description: Default class for the TMS9919 Sound Generator Chip
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

#include <stdlib.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9919.hpp"

DBG_REGISTER ( __FILE__ );

#define CLOCK_FREQUENCY         3579545

cTMS9919::cTMS9919 () :
    m_pSpeechSynthesizer ( NULL ),
    m_LastData ( 0 ),
    m_Frequency (),
    m_Attenuation (),
    m_NoiseColor ( NOISE_WHITE ),
    m_NoiseType ( 0 )
{
    FUNCTION_ENTRY ( this, "cTMS9919 ctor", true );

    m_Attenuation [0] = 0x0F;
    m_Attenuation [1] = 0x0F;
    m_Attenuation [2] = 0x0F;
    m_Attenuation [3] = 0x0F;
}

cTMS9919::~cTMS9919 ()
{
    FUNCTION_ENTRY ( this, "cTMS9919 dtor", true );

    m_pSpeechSynthesizer = NULL;
}

void cTMS9919::SetNoise ( NOISE_COLOR_E color, int type )
{
    FUNCTION_ENTRY ( this, "cTMS9919::SetNoise", true );

    m_NoiseColor = color;
    m_NoiseType  = type;

    switch ( type ) {
        case 0 : m_Frequency [3] = CLOCK_FREQUENCY / 512;   break;
        case 1 : m_Frequency [3] = CLOCK_FREQUENCY / 1024;  break;
        case 2 : m_Frequency [3] = CLOCK_FREQUENCY / 2048;  break;
        case 3 : m_Frequency [3] = m_Frequency [2];         break;
        default :
            DBG_ERROR ( "Invalid noise type selected: " << type );
            break;
    }
}

void cTMS9919::SetFrequency ( int tone, int freq )
{
    FUNCTION_ENTRY ( this, "cTMS9919::SetFrequency", true );

    m_Frequency [ tone ] = freq;
}

void cTMS9919::SetAttenuation ( int tone, int atten )
{
    FUNCTION_ENTRY ( this, "cTMS9919::SetAttenuation", true );

    m_Attenuation [ tone ] = atten;
}

int cTMS9919::SetSpeechSynthesizer ( cTMS5220 *speech )
{
    FUNCTION_ENTRY ( this, "cTMS9919::SetSpeechSynthesizer", true );

    m_pSpeechSynthesizer = speech;

    return -1;
}

void cTMS9919::WriteData ( UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTMS9919::WriteData", true );

    if ( m_LastData & 0xFF00 ) {
        // Handle Generator & Frequency
        int tone    = ( m_LastData & 0x60 ) >> 5;
        int divisor = (( data & 0x3F ) << 4 ) | ( m_LastData & 0x0F );
        if ( divisor != 0 ) {
            SetFrequency ( tone, ( int ) ( CLOCK_FREQUENCY / ( divisor * 32 )));
        }
        m_LastData = 0;
    } else {
        int tone = ( data & 0x60 ) >> 5;
        if ( data & 0x10 ) {
            // Handle Attenuation
            SetAttenuation ( tone, data & 0x0F );
        } else {
            if ( tone == 3 ) {
                // Handle Noise control
                SetNoise (( data & 0x04 ) ? NOISE_WHITE : NOISE_PERIODIC, ( data & 0x03 ));
            } else {
                m_LastData = 0xFF00 | ( UINT8 ) data;
            }
        }
    }
}
