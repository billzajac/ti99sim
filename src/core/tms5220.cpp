//----------------------------------------------------------------------------
//
// File:        tms5220.cpp
// Date:        27-Nov-2000
// Programmer:  Marc Rousseau
//
// Description: Default class for the TMS5220 Speech Synthesizer Chip
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

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "tms5220.hpp"
#include "tms9919.hpp"
#include "tms9900.hpp"
#include "ti994a.hpp"

DBG_REGISTER ( __FILE__ );

extern int verbose;

// RMS Energy values
const int COEFF_ENERGY [0x10] = {
    0, 52, 87, 123, 174, 246, 348, 491, 694, 981, 1385, 1957, 2764, 3904, 5514, 7789
};

const int COEFF_PITCH [0x40] = {
     0,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
    30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  44,  46,  48,
    50,  52,  53,  56,  58,  60,  62,  65,  68,  70,  72,  76,  78,  80,  84,  86,
    91,  94,  98, 101, 105, 109, 114, 118, 122, 127, 132, 137, 142, 148, 153, 159
};

const double COEFF_K1 [0x20] = {
  -0.97850, -0.97270, -0.97070, -0.96680, -0.96290, -0.95900, -0.95310, -0.94140,
  -0.93360, -0.92580, -0.91600, -0.90620, -0.89650, -0.88280, -0.86910, -0.85350,
  -0.80420, -0.74058, -0.66019, -0.56116, -0.44296, -0.30706, -0.15735, -0.00005,
   0.15725,  0.30696,  0.44288,  0.56109,  0.66013,  0.74054,  0.80416,  0.85350
};


const double COEFF_K2 [0x20] = {
  -0.64000, -0.58999, -0.53500, -0.47507, -0.41039, -0.34129, -0.26830, -0.19209,
  -0.11350, -0.03345,  0.04702,  0.12690,  0.20515,  0.28087,  0.35325,  0.42163,
   0.48553,  0.54464,  0.59878,  0.64796,  0.69227,  0.73190,  0.76714,  0.79828,
   0.82567,  0.84965,  0.87057,  0.88875,  0.90451,  0.91813,  0.92988,  0.98830
};

const double COEFF_K3 [0x10] = {
  -0.86000, -0.75467, -0.64933, -0.54400, -0.43867, -0.33333, -0.22800, -0.12267,
  -0.01733,  0.08800,  0.19333,  0.29867,  0.40400,  0.50933,  0.61467,  0.72000
};

const double COEFF_K4 [0x10] = {
  -0.64000, -0.53145, -0.42289, -0.31434, -0.20579, -0.09723,  0.01132,  0.11987,
   0.22843,  0.33698,  0.44553,  0.55409,  0.66264,  0.77119,  0.87975,  0.98830
};

const double COEFF_K5 [0x10] = {
  -0.64000, -0.54933, -0.45867, -0.36800, -0.27733, -0.18667, -0.09600, -0.00533,
   0.08533,  0.17600,  0.26667,  0.35733,  0.44800,  0.53867,  0.62933,  0.72000
};

const double COEFF_K6 [0x10] = {
  -0.50000, -0.41333, -0.32667, -0.24000, -0.15333, -0.06667,  0.02000,  0.10667,
   0.19333,  0.28000,  0.36667,  0.45333,  0.54000,  0.62667,  0.71333,  0.80000
};

const double COEFF_K7 [0x10] = {
  -0.60000, -0.50667, -0.41333, -0.32000, -0.22667, -0.13333, -0.04000,  0.05333,
   0.14667,  0.24000,  0.33333,  0.42667,  0.52000,  0.61333,  0.70667,  0.80000
};

const double COEFF_K8 [0x08] = {
  -0.50000, -0.31429, -0.12857,  0.05714,  0.24286,  0.42857,  0.61429,  0.80000
};

const double COEFF_K9 [0x08] = {
  -0.50000, -0.34286, -0.18571, -0.02857,  0.12857,  0.28571,  0.44286,  0.60000
};

const double COEFF_K10 [0x08] = {
  -0.40000, -0.25714, -0.11429,  0.02857,  0.17143,  0.31429,  0.45714,  0.60000
};

#define CHIRP 22222

#if ( CHIRP == 1 )

static const int chirpTable [25] = {
       8,  -16,   26,  -48,   86,
    -162,  294, -502,  718, -728,
     184,  672, -610, -672,  184,
     728,  718,  502,  294,  162,
      86,   48,   26,   16,    8
};

#elif ( CHIRP == 2 )

static const int chirpTable [30] = { 0x0100 };

#elif ( CHIRP == 3 )

static const int chirpTable [41] = {
    0x00, 0x2A, 0xD4, 0x32, 0xB2, 0x12, 0x25, 0x14, 0x02, 0xE1,
    0xC5, 0x02, 0x5F, 0x5A, 0x05, 0x0F, 0x26, 0xFC, 0xA5, 0xA5,
    0xD6, 0xDD, 0xDC, 0xFC, 0x25, 0x2B, 0x22, 0x21, 0x0F, 0xFF,
    0xF8, 0xEE, 0xED, 0xEF, 0xF7, 0xF6, 0xFA, 0x00, 0x03, 0x02,
    0x01
};

#else

static const char chirpTable [51] = {
    ( char ) 0x00, ( char ) 0x2A, ( char ) 0xD4, ( char ) 0x32,
    ( char ) 0xB2, ( char ) 0x12, ( char ) 0x25, ( char ) 0x14,
    ( char ) 0x02, ( char ) 0xE1, ( char ) 0xC5, ( char ) 0x02,
    ( char ) 0x5F, ( char ) 0x5A, ( char ) 0x05, ( char ) 0x0F,
    ( char ) 0x26, ( char ) 0xFC, ( char ) 0xA5, ( char ) 0xA5,
    ( char ) 0xD6, ( char ) 0xDD, ( char ) 0xDC, ( char ) 0xFC,
    ( char ) 0x25, ( char ) 0x2B, ( char ) 0x22, ( char ) 0x21,
    ( char ) 0x0F, ( char ) 0xFF, ( char ) 0xF8, ( char ) 0xEE,
    ( char ) 0xED, ( char ) 0xEF, ( char ) 0xF7, ( char ) 0xF6,
    ( char ) 0xFA, ( char ) 0x00, ( char ) 0x03, ( char ) 0x02,
    ( char ) 0x01
};

#endif

cTMS5220::cTMS5220 ( cTMS9919 *pSound ) :
    m_SpeechRom ( NULL ),
    m_LoadPointer ( 0 ),
    m_Address ( 0x0000 ),
    m_ChipSelect ( 0x0000 ),
    m_VsmData ( 0 ),
    m_VsmBitsLeft ( 0 ),
    m_FIFO (),
    m_GetIndex ( 0 ),
    m_PutIndex ( 0 ),
    m_BitsLeft ( 0 ),
    m_ReadByte ( false ),
    m_SpeakExternal ( false ),
    m_BufferEmpty ( true ),
    m_TalkStatus ( false ),
    m_Data ( 0x00 ),
    m_Command ( 0x00 ),
    m_StartParams (),
    m_TargetParams (),
    m_InterpolationStage ( 0 ),
    m_Computer ( NULL ),
    m_SoundChip ( pSound ),
    m_PitchIndex ( 0 ),
    m_NonVoicedLevel ( 0.0 ),
    m_FilterHistory (),
    m_RawDataBuffer (),
    m_PlaybackFrequency ( -1 ),
    m_PlaybackInterval ( 0 ),
    m_PlaybackBuffer ( NULL ),
    m_PlaybackSamplesLeft ( 0 ),
    m_PlaybackDataPtr ( NULL )
{
    FUNCTION_ENTRY ( this, "cTMS5220 ctor", true );

    m_SpeechRom = new UINT8 [ 0x8000 ];

    // Default to 0xFF in case there is no ROM so that we won't get locked up.
    memset ( m_SpeechRom, 0, 0x8000 );

    const char *filename = LocateFile ( "spchrom.bin", "roms" );
    if ( filename != NULL ) {
        FILE *file = fopen ( filename, "rb" );
        if ( file != NULL ) {
            if ( fread ( m_SpeechRom, 1, 0x8000, file ) != 0x8000 ) {
                DBG_WARNING ( "Unable to read speech ROM file" );
                m_SpeechRom [0] = 0;
            }
            fclose ( file );
        }
    }

    if ( m_SpeechRom [0] != 0xAA ) {
        if ( verbose >= 1 ) fprintf ( stdout, "A valid speech ROM was not found\n" );
        DBG_WARNING ( "A valid speech ROM was not found" );
        // Create an empty ROM image
        m_SpeechRom [0] = 0xAA;
    } else {
        if ( verbose >= 1 ) fprintf ( stdout, "Using speech ROM \"%s\"\n", filename );
    }

    if ( m_SoundChip != NULL ) {
        m_PlaybackFrequency = m_SoundChip->SetSpeechSynthesizer ( this );
        m_PlaybackInterval  = ( int ) ( m_PlaybackFrequency * 0.003125 );
        m_PlaybackBuffer    = new double [ m_PlaybackInterval ];
    }

    // Calculate the RMS level to use for non-voiced sounds
    double sum = 0.0;
    for ( unsigned  i = 0; i < SIZE ( chirpTable ); i++ ) {
        sum += ( double ) chirpTable [i] * ( double ) chirpTable [i];
    }
    m_NonVoicedLevel = sqrt ( sum / SIZE ( chirpTable ));

    Reset ();
}

cTMS5220::~cTMS5220 ()
{
    FUNCTION_ENTRY ( this, "cTMS5220 dtor", true );

    if ( m_SoundChip != NULL ) {
        m_SoundChip->SetSpeechSynthesizer ( NULL );
    }

    delete [] m_PlaybackBuffer;
    m_PlaybackBuffer = NULL;

    delete [] m_SpeechRom;
    m_SpeechRom = NULL;
}

void cTMS5220::LoadAddress ( UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTMS5220::LoadAddress", true );

    switch ( m_LoadPointer++ ) {
        case 0 :
            m_Address = ( UINT16 ) (( m_Address & 0xFFF0 ) | (( data & 0x0F ) << 0 ));
            break;
        case 1 :
            m_Address = ( UINT16 ) (( m_Address & 0xFF0F ) | (( data & 0x0F ) << 4 ));
            break;
        case 2 :
            m_Address = ( UINT16 ) (( m_Address & 0xF0FF ) | (( data & 0x0F ) << 8 ));
            break;
        case 3 :
            m_Address = ( UINT16 ) (( m_Address & 0xCFFF ) | (( data & 0x03 ) << 12 ));
            m_ChipSelect = ( UINT16 ) (( m_ChipSelect & 0xFFFC ) | (( data & 0x0C ) >> 2 ));
            break;
        case 4 :
            m_ChipSelect = ( UINT16 ) (( m_ChipSelect & 0xFFF3 ) | (( data & 0x03 ) << 2 ));
            DBG_TRACE ( "Chip: " << hex << ( UINT8 ) m_ChipSelect << "  Address: " << hex << m_Address );
            m_LoadPointer = 0;
            m_VsmBitsLeft = 0;
            break;
        default :
            // Just to shut up the compiler - can't get here
            break;
    }
}

void cTMS5220::SaveReadState ( sReadState *state ) const
{
    FUNCTION_ENTRY ( this, "cTMS5220::SaveReadState", true );

    if ( m_SpeakExternal == true ) {
        state->fifo.GetIndex  = m_GetIndex;
        state->fifo.PutIndex  = m_PutIndex;
        state->fifo.BitsLeft  = m_BitsLeft;
    } else {
        state->rom.Address    = m_Address;
        state->rom.ChipSelect = m_ChipSelect;
        state->rom.BitsUsed   = m_VsmBitsLeft;
    }
}

void cTMS5220::RestoreReadState ( const sReadState &state )
{
    FUNCTION_ENTRY ( this, "cTMS5220::RestoreReadState", true );

    DBG_TRACE ( "Restoring read state" );

    if ( m_SpeakExternal == true ) {
        if ( m_PutIndex != state.fifo.PutIndex ) {
            DBG_WARNING ( "FIFO data written while parsing frame" );
        }
        m_GetIndex    = state.fifo.GetIndex;
        m_PutIndex    = state.fifo.PutIndex;
        m_BitsLeft    = state.fifo.BitsLeft;
    } else {
        m_Address     = state.rom.Address;
        m_ChipSelect  = state.rom.ChipSelect;
        m_VsmBitsLeft = state.rom.BitsUsed;
    }
}

static jmp_buf jump_buffer;

UINT8 cTMS5220::ReadBits ( int count )
{
    FUNCTION_ENTRY ( this, "cTMS5220::ReadBits", true );

    return ( m_SpeakExternal == true ) ? ReadBitsFIFO ( count ) : ReadBitsROM ( count );
}

UINT8 cTMS5220::ReadBitsROM ( int count )
{
    FUNCTION_ENTRY ( this, "cTMS5220::ReadBitsROM", true );

    UINT8 data = 0;

    while ( count-- ) {

        if ( m_VsmBitsLeft == 0 ) {
            int address   = ( m_ChipSelect << 14 ) | m_Address;
            m_Address     = ( UINT16 ) ( 0x3FFF & ( m_Address + 1 ));
            m_VsmData     = m_SpeechRom [ address & 0x7FFF ];
            m_VsmBitsLeft = 8;
        }

        data <<= 1;
        if ( m_VsmData & 0x80 ) {
            data |= 1;
        }

        m_VsmData <<= 1;
        m_VsmBitsLeft--;
    }

    return data;
}

UINT8 cTMS5220::ReadBitsFIFO ( int count )
{
    FUNCTION_ENTRY ( this, "cTMS5220::ReadBitsFIFO", true );

    DBG_TRACE ( "Reading " << count << "/" << m_BitsLeft << " bits" );

    // Last chance for the CPU thread...
    for ( int retry = 0; ( m_BitsLeft < count ) && ( retry < 10 ); retry++ ) {
        DBG_TRACE ( "waiting for bits..." );
        if ( m_Computer != NULL ) {
            m_Computer->WakeCPU ( 1 );
        }
    }

    // TBD - Acquire MUTEX

    UINT8 data = 0;

    if ( m_BitsLeft < count ) {

        DBG_ERROR ( "Not enough bits (" << m_BitsLeft << ") left in the FIFO - " << count << " needed" );

        // TBD - Release MUTEX

        longjmp ( jump_buffer, -1 );

    } else {

        // Get the # of bits left in the current byte
        int bitsLeft = (( m_BitsLeft - 1 ) % 8 ) + 1;

        m_BitsLeft -= count;

        UINT8 fifoData = ( UINT8 ) ( m_FIFO [ m_GetIndex ] >> ( 8 - bitsLeft ));

        if ( bitsLeft <= count ) {
            count -= bitsLeft;
            while ( bitsLeft-- ) {
                data = ( UINT8 ) (( data << 1 ) | ( fifoData & 1 ));
                fifoData >>= 1;
            }
            m_GetIndex = ( m_GetIndex + 1 ) % FIFO_BYTES;
            fifoData = m_FIFO [ m_GetIndex ];
        }

        while ( count-- ) {
            data = ( UINT8 ) (( data << 1 ) | ( fifoData & 1 ));
            fifoData >>= 1;
        }
    }

    // TBD - Release MUTEX

    return data;
}

void cTMS5220::StoreDataFIFO ( UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTMS5220::StoreDataFIFO", true );

    // TBD - Acquire MUTEX
    m_FIFO [ m_PutIndex ] = data;

//data = (( data >> 1 ) & 0x55 ) | (( data << 1 ) & 0xAA );
//data = (( data >> 2 ) & 0x33 ) | (( data << 2 ) & 0xCC );
//data = (( data >> 4 ) & 0x0F ) | (( data << 4 ) & 0xF0 );
//fprintf ( stderr, " %02X", data );

    m_BitsLeft += 8;

    int nextIndex = ( m_PutIndex + 1 ) % FIFO_BYTES;

    if (( m_PlaybackFrequency != -1 ) && ( nextIndex == m_GetIndex )) {
        DBG_TRACE ( "FIFO full - stalling CPU... (" << m_PutIndex << "/" << m_GetIndex << ")" );
        while ( nextIndex == m_GetIndex ) {
            if ( m_Computer != NULL ) {
                // TBD - calculate a good approximation for the # of CPU cycles to stall
                m_Computer->Sleep ( 100, 1 );
            }
            // Check TalkStatus in case we stalled
            if ( m_TalkStatus == false ) break;
        }
        DBG_TRACE ( "CPU stall complete... (" << m_PutIndex << "/" << m_GetIndex << ")" );
    }

    m_PutIndex = nextIndex;

    if (( m_TalkStatus == false ) && ( m_PutIndex >= 9 )) {
        m_TalkStatus = true;
    }

    m_BufferEmpty = false;

    // TBD - Release MUTEX

    // If we aren't getting audio callbacks, try to empty the buffer
    if (( m_PlaybackFrequency == -1 ) && ( m_TalkStatus == true )) {
        sSpeechParams temp;
        ReadFrame ( &temp, true );
    }
}

#if 0
static void deemp ( double *x, int count )
{
    static double dei [4] = { 0, 0, 0, 0 };
    static double deo [4] = { 0, 0, 0, 0 };

    for ( int i = 0; i < count; i++ ) {
        dei [0] = x [i];
        x [i] = dei [0] - dei [1] * 1.9998f + dei [2] + deo [1] * 2.5f - deo [2] * 2.0925f + deo [3] * .585f;
        dei [2] = dei [1];
        dei [1] = dei [0];
        deo [3] = deo [2];
        deo [2] = deo [1];
        deo [1] = x [i];
x[i] *= 0.5;
    }
}
#endif

bool cTMS5220::CreateNextBuffer ()
{
    FUNCTION_ENTRY ( this, "cTMS5220::CreateNextBuffer", true );

    if ( m_InterpolationStage == 0 ) {

        // Try to get the next set of parameters
        sSpeechParams temp;
        if ( ReadFrame ( &temp, true ) == false ) {
            DBG_TRACE ( "** UNDER-RUN **" );
            return false;
        }

        // Update the start & target parameters
        memcpy ( &m_StartParams, &m_TargetParams, sizeof ( sSpeechParams ));

        // TBD: SILENCE - copy all parameters except energy

        if (( temp.Repeat == false ) && ( temp.Stop == false )) {
            memcpy ( &m_TargetParams, &temp, sizeof ( sSpeechParams ));
        } else {
            m_TargetParams.Energy = temp.Energy;
            if ( temp.Stop == false ) {
                m_TargetParams.Pitch  = temp.Pitch;
            }
        }

        m_TargetParams.Stop = temp.Stop;
    }

    sSpeechParams param;
    InterpolateParameters ( m_InterpolationStage, m_StartParams, m_TargetParams, &param );
    m_InterpolationStage = ( m_InterpolationStage + 1 ) % 8;

    if ( m_PitchIndex >= param.Pitch ) {
        m_PitchIndex = 0;
    }

    for ( int i = 0; i < INTERPOLATION_INTERVAL; i++ ) {

        double sample = 0.0;

        if ( param.Pitch == 0 ) {
//            double random = ( rand () % 32768 ) / 16384.0 - 1.0;
//            sample = m_NonVoicedLevel * random;
//sample *= 0.25;
            sample = ( rand () & 1 ) ? 64 : -64;
        } else {
            if ( m_PitchIndex < ( int ) SIZE ( chirpTable )) {
                sample = chirpTable [ m_PitchIndex ];
//sample *= param.Gain;
            }
            m_PitchIndex = ( m_PitchIndex + 1 ) % param.Pitch;
        }
sample *= param.Gain * 0.1;

        // Synthesize...
//        m_FilterHistory [ 0 ][ RC_ORDER ] = sample * param.Gain;
        m_FilterHistory [ 0 ][ RC_ORDER ] = sample;

        // Forward path
        for ( int j = RC_ORDER - 1; j >= 0; j-- ) {
            m_FilterHistory [ 0 ][ j ] = m_FilterHistory [ 0 ][ j + 1 ] - param.Reflection [ j ] * m_FilterHistory [ 1 ][ j ];
        }

        // Backward path
        for ( int j = RC_ORDER - 1; j >= 1; j-- ) {
            m_FilterHistory [ 1 ][ j ] = m_FilterHistory [ 1 ][ j - 1 ] + param.Reflection [ j - 1 ] * m_FilterHistory [ 0 ][ j - 1 ];
        }

//        m_RawDataBuffer [ i ] = m_FilterHistory [ 1 ][ 0 ] = m_FilterHistory [ 0 ][ 0 ];
        double cliptemp = m_FilterHistory [ 1 ][ 0 ] = m_FilterHistory [ 0 ][ 0 ];

        DBG_TRACE ( "Data: " << cliptemp );

        if (cliptemp > 2047) cliptemp = -2048 + (cliptemp-2047);
        else if (cliptemp < -2048) cliptemp = 2047 - (cliptemp+2048);

        if (cliptemp > 511)
            m_RawDataBuffer [ i ] = 127;//<<8;
        else if (cliptemp < -512)
            m_RawDataBuffer [ i ] = -128;//<<8;
        else
            m_RawDataBuffer [ i ] = cliptemp / 4 ;//* 64;//<< 6;

    }

//    deemp ( m_RawDataBuffer, INTERPOLATION_INTERVAL );

    if (( m_InterpolationStage == 0 ) && ( m_TargetParams.Stop == true )) {
        m_BufferEmpty   = false;
        m_TalkStatus    = false;
        m_SpeakExternal = false;
    }

    return true;
}

bool cTMS5220::ConvertBuffer ()
{
    FUNCTION_ENTRY ( this, "cTMS5220::ConvertBuffer", true );

    // TBD - Convert buffer from 8KHz to m_PlaybackFrequency here

    // NOTE: This is a simple way to do it - linearly interpolate the samples (introduces some high-frequecy artifacts)
    double ratio = ( double ) m_PlaybackInterval / ( double ) INTERPOLATION_INTERVAL;
    double count = ratio;
    int j = 0;
    static double last = 0.0;
    for ( int i = 0; i < INTERPOLATION_INTERVAL; i++ ) {
        double next = m_RawDataBuffer [i];
        double max  = ( int ) count;
        while ( count >= 1.0 ) {
            double x = ( int ) count / max;
            m_PlaybackBuffer [j++] = x * last + ( 1.0 - x ) * next;
            count -= 1.0;
        }
        last = next;
        count += ratio;
    }

    // Finish what's left over (will be less than n=ratio samples)
    while ( j < m_PlaybackInterval ) {
        m_PlaybackBuffer [j++] = m_RawDataBuffer [INTERPOLATION_INTERVAL-1];
    }

    last = m_RawDataBuffer [INTERPOLATION_INTERVAL-1];

    return true;
}

bool cTMS5220::GetNextBuffer ()
{
    FUNCTION_ENTRY ( this, "cTMS5220::GetNextBuffer", true );

    if ( CreateNextBuffer () == false ) {
        return false;
    }

    if ( ConvertBuffer () == false ) {
        return false;
    }

    m_PlaybackDataPtr     = m_PlaybackBuffer;
    m_PlaybackSamplesLeft = m_PlaybackInterval;

    return true;
}

char *cTMS5220::FormatParameters ( const sSpeechParams &param, bool showAll )
{
    FUNCTION_ENTRY ( NULL, "cTMS5220::FormatParameters", true );

    static char buffer [256];

    char *ptr = buffer;

    ptr += sprintf ( ptr, "En: %4d  Rpt: %d  Pi: %3d", param.Energy, param.Repeat, param.Pitch );

    if (( showAll == true ) || ( param.Repeat == false )) {
        int max = (( showAll == true ) || ( param.Pitch != 0 ))? 10 : 4;
        ptr += sprintf ( ptr, "  K:" );
        for ( int i = 0; i < max; i++ ) {
            ptr += sprintf ( ptr, " %8.5f", param.Reflection [i] );
        }
    }

    return buffer;
}

/*
void cTMS5220::InterpolateParameters ( int stage, const sSpeechParams &start, const sSpeechParams &end, sSpeechParams *param )
{
    FUNCTION_ENTRY ( NULL, "cTMS5220::InterpolateParameters", true );

    memcpy ( param, &start, sizeof ( sSpeechParams ));

    param->Energy = start.Energy + stage * ( end.Energy - start.Energy ) / 8;
    param->Pitch  = start.Pitch + stage * ( end.Pitch - start.Pitch ) / 8;

    if ( end.Pitch == 0 ) {
        param->Pitch = start.Pitch;
    } else if ( start.Pitch == 0 ) {
        param->Pitch = 0;
    }

    double gain = 1.0;
    for ( int i = 0; i < RC_ORDER; i++ ) {
        param->Reflection [i] = start.Reflection [i] + stage * ( end.Reflection [i] - start.Reflection [i] ) / 8;
        gain /= ( 1 - ( param->Reflection [i] * param->Reflection [i] ));
    }

//    param->Gain = 0.01 * sqrt ( param->Energy / sqrt ( gain ));
    param->Gain = 0.001 * sqrt ( param->Energy / sqrt ( gain )) * sqrt (( double ) param->Pitch );

//fprintf ( stderr, "%s  gain: %15.10f\n", FormatParameters ( *param, true ), 1.0 / gain );
}
*/

void cTMS5220::InterpolateParameters ( int stage,  sSpeechParams &start, const sSpeechParams &end, sSpeechParams *param )
{
    FUNCTION_ENTRY ( NULL, "cTMS5220::InterpolateParameters", true );

    memcpy ( param, &start, sizeof ( sSpeechParams ));

    static int X [8] = { 8, 8, 8, 4, 4, 2, 2, 1 };

    param->Energy += ( end.Energy - param->Energy ) / X[stage];
    param->Pitch  += ( end.Pitch - param->Pitch ) / X[stage];

    if ( end.Pitch == 0 ) {
        param->Pitch = start.Pitch;
    } else if ( start.Pitch == 0 ) {
        param->Pitch = 0;
    }

    for ( int i = 0; i < RC_ORDER; i++ ) {
        param->Reflection [i] += ( end.Reflection [i] - param->Reflection [i] ) / X[stage];
    }

    param->Gain = param->Energy / 256.0;

    DBG_TRACE ( FormatParameters ( *param, false ));

    memcpy ( &start, param, sizeof ( sSpeechParams ));
}

bool cTMS5220::ReadFrame ( sSpeechParams *info, bool restore )
{
    FUNCTION_ENTRY ( this, "cTMS5220::ReadFrame", true );

    memset ( info, 0, sizeof ( sSpeechParams ));

    sReadState state;
    SaveReadState ( &state );

    if ( setjmp ( jump_buffer ) != 0 ) {
        if ( restore == true ) {
            RestoreReadState ( state );
        } else {
            Reset ();
        }
        return false;
    }

    int index = ReadBits ( 4 );

    if ( index == 15 ) {

        DBG_TRACE ( "--Stop--" );

        info->Stop = true;

        return true;

    } else {

        info->Energy = COEFF_ENERGY [ index ];

        if ( index != 0 ) {

            info->Repeat = ReadBits ( 1 ) ? true : false;
            info->Pitch  = COEFF_PITCH [ ReadBits ( 6 )];

            if ( info->Repeat == false ) {

                info->Reflection [ REFLECTION_K1 ] = COEFF_K1 [ ReadBits ( 5 )];
                info->Reflection [ REFLECTION_K2 ] = COEFF_K2 [ ReadBits ( 5 )];
                info->Reflection [ REFLECTION_K3 ] = COEFF_K3 [ ReadBits ( 4 )];
                info->Reflection [ REFLECTION_K4 ] = COEFF_K4 [ ReadBits ( 4 )];

                if ( info->Pitch != 0 ) {

                    info->Reflection [ REFLECTION_K5 ]  = COEFF_K5 [ ReadBits ( 4 )];
                    info->Reflection [ REFLECTION_K6 ]  = COEFF_K6 [ ReadBits ( 4 )];
                    info->Reflection [ REFLECTION_K7 ]  = COEFF_K7 [ ReadBits ( 4 )];
                    info->Reflection [ REFLECTION_K8 ]  = COEFF_K8 [ ReadBits ( 3 )];
                    info->Reflection [ REFLECTION_K9 ]  = COEFF_K9 [ ReadBits ( 3 )];
                    info->Reflection [ REFLECTION_K10 ] = COEFF_K10 [ ReadBits ( 3 )];

                }

            }

            DBG_TRACE ( FormatParameters ( *info, false ));

        } else {

            DBG_TRACE ( "--Silence--" );

        }

    }

    if ( m_SpeakExternal == true ) {
        if ( m_BitsLeft == 0 ) {
            m_BufferEmpty = true;
        }
    }

    return true;
}

void cTMS5220::SetComputer ( cTI994A *computer )
{
    FUNCTION_ENTRY ( this, "cTMS5220::SetComputer", true );

    m_Computer = computer;
}

bool cTMS5220::AudioCallback ( UINT8 *buffer, int count )
{
    FUNCTION_ENTRY ( this, "cTMS5220::AudioCallback", false );

    if ( m_TalkStatus == false ) {
        return false;
    }

    bool modified = false;

    while (( count > 0 ) && ( m_TalkStatus == true )) {

        if (( m_PlaybackSamplesLeft == 0 ) && ( GetNextBuffer () == false )) {
            break;
        }

        modified = true;

        int size = min ( count, m_PlaybackSamplesLeft );
        for ( int i = 0; i < size; i++ ) {
            int sample = ( int ) ( *buffer - 128 + *m_PlaybackDataPtr++ );
            *buffer++ = 128 + min ( 127, max ( -128, sample ));
        }

        count -= size;
        m_PlaybackSamplesLeft -= size;
    }

    if ( m_TalkStatus == false ) {
        Reset ();
    }

    return modified;
}

void cTMS5220::Reset ()
{
    FUNCTION_ENTRY ( this, "cTMS5220::Reset", true );

    DBG_TRACE ( "Resetting the TMS5220" );

    memset ( m_FIFO, 0, sizeof ( m_FIFO ));

    m_GetIndex      = 0;
    m_PutIndex      = 0;
    m_BitsLeft      = 0;

    m_ReadByte      = false;
    m_SpeakExternal = false;

    m_BufferEmpty   = true;
    m_TalkStatus    = false;

    m_Data          = 0x00;
    m_Command       = 0x00;

    m_LoadPointer   = 0;
    m_Address       = 0x0000;
    m_ChipSelect    = 0x0000;
    m_VsmData       = 0;
    m_VsmBitsLeft   = 0;

    memset ( &m_StartParams, 0, sizeof ( m_StartParams ));
    memset ( &m_TargetParams, 0, sizeof ( m_TargetParams ));

    memset ( m_FilterHistory, 0, sizeof ( m_FilterHistory ));
    memset ( m_RawDataBuffer, 0, sizeof ( m_RawDataBuffer ));

    m_InterpolationStage  = 0;
    m_PlaybackSamplesLeft = 0;
}

UINT8 cTMS5220::WriteData ( UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTMS5220::WriteData", false );

    if ( m_SpeakExternal == true ) {

        DBG_TRACE ( "External data: " << hex << data );

        StoreDataFIFO ( data );

    } else {

        switch ( data & SPEECH_COMMAND_MASK ) {
            case 0x00 : // Load-Frame-Rate
                DBG_WARNING ( "CMD: Load-Frame-Rate ** Unsupported **" );
                break;
            case 0x20 :
                DBG_WARNING ( "Unsupported command received" );
                break;
            case 0x10 : // Read-Byte
                DBG_TRACE ( "CMD: Read-Byte" );
                m_ReadByte = true;
                break;
            case 0x30 : // Read-and-Branch
                m_Address  = ReadBits ( 8 ) << 8;
                m_Address |= ReadBits ( 8 );
                m_Address &= 0x3FFF;
                DBG_TRACE ( "CMD: Read-and-Branch - Address = " << hex << m_Address );
                break;
            case 0x40 : // Load-Address (expect 4 more)
                DBG_TRACE ( "CMD: Load-Address" );
                LoadAddress ( data );
                break;
            case 0x50 : // Speak
                DBG_TRACE ( "CMD: Speak" );
                m_TalkStatus = true;
                break;
            case 0x60 : // Speak-External (accepts an unlimited number of data bytes)
                DBG_TRACE ( "CMD: Speak-External" );
                m_SpeakExternal = true;
                break;
            case 0x70 : // Reset
                Reset ();
                break;
            default :
                // Just to shut up the compiler - can't get here
                break;
        }
    }

    return data;
}

UINT8 cTMS5220::ReadData ( UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTMS5220::ReadData", false );

    if ( m_ReadByte == true ) {
        m_ReadByte = false;
        data = ReadBitsROM ( 8 );
        DBG_TRACE ( "READ: " << hex << data << " (" << ( char ) ( isprint ( data ) ? data : '.' ) << ")" );
    } else {
        int bufferLow = ( m_BitsLeft < FIFO_BITS / 2 ) ? 1 : 0;
        data = ( m_TalkStatus << 7 ) | ( bufferLow << 6 ) | ( m_BufferEmpty << 5 );
        DBG_TRACE ( "Status: " << hex << data );
    }

    return data;
}
