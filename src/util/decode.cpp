//----------------------------------------------------------------------------
//
// File:        decode.cpp
// Date:        10-Aug-2000
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2000-2010 Marc Rousseau, All Rights Reserved.
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

#if ( BYTE_ORDER == BIG_ENDIAN )
    #define SWAP_ENDIAN_16(x)   (((( x ) & 0xFF ) << 8 ) | ((( x ) >> 8 ) & 0xFF ))
    #define SWAP_ENDIAN_32(x)   (( SWAP_ENDIAN_16 ( x ) << 16 ) | SWAP_ENDIAN_16 ( x >> 16 ))
#else
    #define SWAP_ENDIAN_16(x)   x
    #define SWAP_ENDIAN_32(x)   x
#endif

#define WAVE_FORMAT_PCM     1

const char CHUNK_RIFF [] = "RIFF";
const char CHUNK_WAVE [] = "WAVE";
const char CHUNK_FMT []  = "fmt ";
const char CHUNK_DATA [] = "data";

struct sRIFF_Block
{
    char   Tag [ 4 ];
    UINT32 Length;
};

struct sFormatChunk : sRIFF_Block
{
    UINT16    wFormatTag;
    UINT16    nChannels;
    UINT32    nSamplesPerSec;
    UINT32    nAvgBytesPerSec;
    UINT16    nBlockAlign;
    UINT16    wBitsPerSample;
};

struct GUID
{
    UINT32    Data1;
    UINT16    Data2;
    UINT16    Data3;
    UINT8     Data4 [8];
};

struct sFormatChunkEx : sFormatChunk
{
    UINT16    cbSize;
    UINT16    wValidBitsPerSample;
    UINT32    dwChannelMask;
    GUID      SubFormat;
};

struct sPreambleData
{
    double         threshold;
    double         avgPeriod;
    double         minPeriod;
    double         maxPeriod;
};

struct sFileData
{
    const UINT8   *pData;
    UINT32         dataLength;
    double         period;
    double         minPeriod;
    double         maxPeriod;
};

sFormatChunkEx fmt;

void CopyFormatChunk ( sFormatChunkEx *pFormat, const sFormatChunkEx *pRawChunk )
{
    FUNCTION_ENTRY ( NULL, "CopyFormatChunk", true );

    memset ( pFormat, 0, sizeof ( sFormatChunkEx ));

    memcpy ( &pFormat->Tag, &pRawChunk->Tag, 4 );
    pFormat->Length              = SWAP_ENDIAN_32 ( pRawChunk->Length );
    pFormat->wFormatTag          = SWAP_ENDIAN_16 ( pRawChunk->wFormatTag );
    pFormat->nChannels           = SWAP_ENDIAN_16 ( pRawChunk->nChannels );
    pFormat->nSamplesPerSec      = SWAP_ENDIAN_32 ( pRawChunk->nSamplesPerSec );
    pFormat->nAvgBytesPerSec     = SWAP_ENDIAN_32 ( pRawChunk->nAvgBytesPerSec );
    pFormat->nBlockAlign         = SWAP_ENDIAN_16 ( pRawChunk->nBlockAlign );
    pFormat->wBitsPerSample      = SWAP_ENDIAN_16 ( pRawChunk->wBitsPerSample );
    pFormat->cbSize              = SWAP_ENDIAN_16 ( pRawChunk->cbSize );
    if ( pFormat->cbSize == sizeof ( sFormatChunkEx ))
    {
        pFormat->wValidBitsPerSample = SWAP_ENDIAN_16 ( pRawChunk->wValidBitsPerSample );
        pFormat->dwChannelMask       = SWAP_ENDIAN_32 ( pRawChunk->dwChannelMask );
        memcpy ( &pFormat->SubFormat, &pRawChunk->SubFormat, sizeof ( GUID ));
    }
}

static bool CheckHeader ( UINT8 *pFileData, int fileSize, UINT8 **pData, int *pDataSize, sFormatChunkEx *pFormat )
{
    FUNCTION_ENTRY ( NULL, "CheckHeader", true );

    sRIFF_Block *pRIFF = ( sRIFF_Block * ) pFileData;

    *pData     = NULL;
    *pDataSize = 0;
    memset ( pFormat, 0, sizeof ( sFormatChunkEx ));

    if ( strncmp ( pRIFF->Tag, CHUNK_RIFF, 4 ) != 0 )
    {
        return false;
    }

    do
    {
        UINT32 riffLength = SWAP_ENDIAN_32 ( pRIFF->Length );

        char *ptr = ( char * ) ( pRIFF + 1 );

        sFormatChunkEx *fmt = NULL;
        sRIFF_Block   *data = NULL;

        if ( strncmp ( ptr, CHUNK_WAVE, 4 ) == 0 )
        {
            ptr += 4;
            do
            {
                sRIFF_Block *pChunk = ( sRIFF_Block * ) ptr;
                UINT32 length = SWAP_ENDIAN_32 ( pChunk->Length );
                if ( strncmp ( pChunk->Tag, CHUNK_FMT, 4 ) == 0 )
                {
                    fmt = ( sFormatChunkEx * ) pChunk;
                    CopyFormatChunk ( pFormat, fmt );
                }
                else if ( strncmp ( pChunk->Tag, CHUNK_DATA, 4 ) == 0 )
                {
                    data = ( sRIFF_Block * ) pChunk;
                }
                if (( fmt != NULL ) && ( data != NULL ))
                {
                    *pData     = ( UINT8 * ) ( data + 1 );
                    *pDataSize = length;
                    return true;
                }
                ptr = ( char * ) ( pChunk + 1 ) + (( length + 1 ) & 0xFFFFFFFE );
            }
            while ( ptr < (( char * ) ( pRIFF + 1 ) + riffLength ));
        }

        pRIFF = ( sRIFF_Block * ) (( UINT8 * ) ( pRIFF + 1 ) + (( riffLength + 1 ) & 0xFFFFFFFE ));

    }
    while (( UINT8 * ) pRIFF < pFileData + fileSize );

    return false;
}

int (*GetSample) ();

const UINT8 *tapeBuffer;
const UINT8 *tapeStart;
const UINT8 *tapeEnd;

#if ( BYTE_ORDER == LITTLE_ENDIAN )

    static int GetSampleMono8 ()
    {
        return * ( UINT8 * ) tapeBuffer++ - 0x80;
    }

    static int GetSampleMono16 ()
    {
        return * (( INT16 *& ) tapeBuffer )++;
    }

    static int GetSampleMono32 ()
    {
        return * (( INT32 *& ) tapeBuffer )++;
    }

/*
    static int GetSampleMono32float_16_8 ()
    {
        return ( int ) * (( float *& ) tapeBuffer )++;
    }
*/

    static int GetSampleStereo8 ()
    {
        int value = * ( UINT8 * ) tapeBuffer - 0x80;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }

    static int GetSampleStereo16 ()
    {
        int value = * ( INT16 * ) tapeBuffer;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }

    static int GetSampleStereo32 ()
    {
        int value = * ( INT32 * ) tapeBuffer;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }

/*
    static int _GetSampleStereo8 ()
    {
        int value = * ( UINT8 * ) tapeBuffer - 0x80;
        value >>= 8 - fmt.wValidBitsPerSample;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }

    static int _GetSampleStereo16 ()
    {
        int value = * ( INT16 * ) tapeBuffer;
        value >>= 16 - fmt.wValidBitsPerSample;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }

    static int _GetSampleStereo32 ()
    {
        int value = * ( INT32 * ) tapeBuffer;
        value >>= 32 - fmt.wValidBitsPerSample;
        tapeBuffer += fmt.nBlockAlign;
        return value;
    }
*/

#endif

#if ( BYTE_ORDER == BIG_ENDIAN )

    static int GetSampleMono8 ()
    {
        return * ( UINT8 * ) tapeBuffer++ - 0x80;
    }

    static int GetSampleMono16 ()
    {
        INT16 retVal = *tapeBuffer++;
        retVal |= ( *tapeBuffer++ << 8 );
        return retVal;
    }

    static int GetSampleMono32 ()
    {
        INT32 retVal = *tapeBuffer++;
        retVal |= ( *tapeBuffer++ << 8 );
        retVal |= ( *tapeBuffer++ << 16 );
        retVal |= ( *tapeBuffer++ << 24 );
        return retVal;
    }

    static int GetSampleStereo8 ()
    {
        int retVal = tapeBuffer [0] - 0x80;
        tapeBuffer += fmt.nBlockAlign;
        return retVal;
    }

    static int GetSampleStereo16 ()
    {
        INT16 retVal = tapeBuffer [0];
        retVal |= ( tapeBuffer [1] << 8 );
        tapeBuffer += fmt.nBlockAlign;
        return retVal;
    }

    static int GetSampleStereo32 ()
    {
        INT32 retVal = tapeBuffer [0];
        retVal |= ( tapeBuffer [1] << 8 );
        retVal |= ( tapeBuffer [2] << 16 );
        retVal |= ( tapeBuffer [3] << 24 );
        tapeBuffer += fmt.nBlockAlign;
        return retVal;
    }

#endif

//----------------------------------------------------------------------------
//
// Find the next zero-crossing of the audio signal.
//
// TBD: Add a low-pass filter to try to compensate for a varying DC bias
//
//----------------------------------------------------------------------------

double bias;

static double FindNextCrossing ()
{
    FUNCTION_ENTRY ( NULL, "FindNextCrossing", false );

    static int lastValue = GetSample ();
    static double lastDistance = 0.0;

    const UINT8 *pStart = tapeBuffer;

    int value = 0;

    if ( lastValue < bias )
    {
        while ( tapeBuffer < tapeEnd )
        {
            value = GetSample ();
            if ( value > bias + 1.0 ) break;
            lastValue = value;
        }
    }
    else
    {
        while ( tapeBuffer < tapeEnd )
        {
            value = GetSample ();
            if ( value < bias - 1.0 ) break;
            lastValue = value;
        }
    }

    if ( tapeBuffer >= tapeEnd ) return -1;

    double fraction = - ( double ) ( lastValue - bias ) / ( double ) ( value - lastValue );
    double distance = lastDistance + ( tapeBuffer - pStart ) / fmt.nBlockAlign - 1 + fraction;

    lastDistance = 1.0 - fraction;
    lastValue    = value;

    return distance;
}

typedef int (*QSORT_FUNC) ( const void *, const void * );

static int sortPreamble ( const sPreambleData *p1, const sPreambleData *p2 )
{
    FUNCTION_ENTRY ( NULL, "sortPreamble", false );

    if ( p1->threshold < p2->threshold ) return -1;
    if ( p1->threshold > p2->threshold ) return 1;

    if ( p1->minPeriod < p2->minPeriod ) return -1;
    if ( p1->minPeriod > p2->minPeriod ) return 1;

    return ( p1->maxPeriod > p2->maxPeriod ) ? -1 : 1;
}

static void StoreSection ( sFileData *fileData, sPreambleData *preamble, int preSize, const UINT8 *start )
{
    FUNCTION_ENTRY ( NULL, "StoreSection", true );

    qsort ( preamble, preSize, sizeof ( sPreambleData ), ( QSORT_FUNC ) sortPreamble );

    int periodCount = 0;
    double periodSum = 0.0;

    fileData->minPeriod = preamble [0].avgPeriod;
    fileData->maxPeriod = preamble [0].avgPeriod;

    while ( periodCount < preSize )
    {
        if ( preamble->threshold > 0.002 ) break;
        if ( preamble->minPeriod < fileData->minPeriod ) fileData->minPeriod = preamble->minPeriod;
        if ( preamble->maxPeriod > fileData->maxPeriod ) fileData->maxPeriod = preamble->maxPeriod;
        periodSum += preamble->avgPeriod;
        preamble++;
        periodCount++;
    }

    if ( periodCount < preSize / 4 ) fprintf ( stdout, "Warning: Preamble quality is questionable (%f%% valid)\n", 100.0 * periodCount / preSize );

    fileData->dataLength = start - fileData->pData;
    fileData->period     = periodSum / periodCount;
}

static int FormatTime ( char *buffer, double time )
{
    FUNCTION_ENTRY ( NULL, "FormatTime", true );

    int minutes = ( int ) ( time / 60.0 );
    time -= minutes * 60;
    int seconds = ( int ) ( time );
    time -= seconds;
    int msecs   = ( int ) ( time * 1000.0 );

    return sprintf ( buffer, "%02d:%02d.%03d", minutes, seconds, msecs );
}

static void PrintHeader ()
{
    fprintf ( stdout, "          Start time  End time    Approx size\n" );
}

static void PrintSection ( sFileData *data, int index )
{
    char startTime [16], endTime [16];
    FormatTime ( startTime, ( double ) ( data->pData - tapeStart ) / ( double ) fmt.nAvgBytesPerSec );
    FormatTime ( endTime, ( double ) ( data->pData + data->dataLength - tapeStart ) / ( double ) fmt.nAvgBytesPerSec );

    int dataSize = ( int ) ( data->dataLength / fmt.nBlockAlign / data->period / 8 );
    dataSize -= 0x300 + 1 + 2;
    dataSize /= 2 * ( 8 + 1 + 64 + 1 );
    dataSize *= 64;
    fprintf ( stdout, "Track %2d: %s - %s   %5d bytes (%02X)\n", index + 1, startTime, endTime, dataSize, dataSize / 64 );
}

//----------------------------------------------------------------------------
//
//  This function attempts to find the relevant data tracks within the file.
//  It looks for the pattern of leading 0's to find the start of the data track.
//  If enough are found and patterns of 1's and 0's are found (determined by
//  the amount of variance), it will switch to data mode.  Once valid data ends
//  (signalled by 'large' variances), the end of the data track is recorded.
//
//----------------------------------------------------------------------------

static int LocateTracks ( UINT8 *pData, UINT32 maxIndex, sFileData *fileData )
{
    FUNCTION_ENTRY ( NULL, "LocateTracks", true );

    const int GROUPING_SIZE    = 50;
    const int HEADER_SIZE      = 8 * 0x300;
    const int HEADER_THRESHOLD = 3 * HEADER_SIZE / 5;

    enum
    {
        STAGE_UNKNOWN,
        STAGE_HEADER,
        STAGE_DATA
    }
    stage = STAGE_UNKNOWN;

    sPreambleData *preamble = new sPreambleData [ maxIndex / GROUPING_SIZE + 1 ];

    // Set up the global pointer used by FindNextCrossing & GetSample routines
    tapeBuffer = pData;
    tapeStart  = pData;
    tapeEnd    = tapeStart + maxIndex;

    int count   = 0;
    int preSize = -1, bad = 0;

    const UINT8 *thisStart = pData;

    fprintf ( stdout, "Searching for data tracks...\n\n" );

    fileData [ count ].minPeriod = maxIndex;

    do
    {
        double sum     = 0;
        double squares = 0;

        thisStart = tapeBuffer;

        double minPeriod = maxIndex;
        double maxPeriod = -1;

        for ( int i = 0; i < GROUPING_SIZE; i++ )
        {
            double dist = FindNextCrossing ();
            if ( dist == -1 ) goto done;
            if ( dist < minPeriod ) minPeriod = dist;
            if ( dist > maxPeriod ) maxPeriod = dist;
            sum     += dist;
            squares += dist * dist;
        }

        double avg = ( double ) sum / ( double ) GROUPING_SIZE;
        double var = ( double ) squares / GROUPING_SIZE - avg * avg;

        if ( verbose >= 3 ) fprintf ( stdout, "var = %12.5f  avg = %12.5f - %12.5f (%3d)\n", var, avg, var / ( avg * avg ), preSize );

        double threshold = var / ( avg * avg );

        switch ( stage )
        {
            case STAGE_UNKNOWN  :

                // Look for a steady pattern of 0's
                if ( threshold >= 0.005 ) break;

                if ( verbose >= 3 ) fprintf ( stdout, "Start of header detected\n" );

                stage = STAGE_HEADER;

                fileData [ count ].pData = thisStart;

                bad     = 0;
                preSize = 0;
                // fall through to the header logic

            case STAGE_HEADER :

                if ( threshold  < 0.005 )
                {
                    // Store this information for the period calculations
                    preamble [preSize].threshold = threshold;
                    preamble [preSize].avgPeriod = avg;
                    preamble [preSize].minPeriod = minPeriod;
                    preamble [preSize].maxPeriod = maxPeriod;
                    preSize++;
                }
                else if ( threshold  < 0.5 )
                {
                    if ( preSize > HEADER_THRESHOLD / GROUPING_SIZE )
                    {
                        // Found the start of data
                        if ( verbose >= 3 ) fprintf ( stdout, "Start of data detected\n" );
                        stage = STAGE_DATA;
                        bad   = 0;
                    }
                    else
                    {
                        if ( threshold < 0.2 )
                        {
                            // Probably just decoding errors
                            if ( verbose >= 3 ) fprintf ( stdout, "-- noise in header detected\n" );
                        }
                        else
                        {
                            // Too much noise - restart the header
                            if ( verbose >= 3 ) fprintf ( stdout, "Found data but header is too short(%d/%d)\n", preSize, HEADER_THRESHOLD / GROUPING_SIZE );
//                            preSize = 0;
                        }
                    }
                }
                else
                {
                    // We've lost the signal
                    if ( verbose >= 3 ) fprintf ( stdout, "No data found\n" );
                    stage = STAGE_UNKNOWN;
                }
                break;

            case STAGE_DATA     :

                if ( threshold  < 0.5 )
                {
                    bad = 0;
                }
                else if ( bad++ > 1 )
                {
                    if ( count == 0 ) PrintHeader ();
                    StoreSection ( &fileData [count], preamble, preSize, thisStart );
                    PrintSection ( &fileData [count], count );
                    count++;
                    stage   = STAGE_UNKNOWN;
//                    preSize = -1;
                }
                break;
        }

    }
    while ( true );

done:

    if ( stage == STAGE_DATA )
    {
        if ( count == 0 ) PrintHeader ();
        StoreSection ( &fileData [count], preamble, preSize, thisStart );
        PrintSection ( &fileData [count], count );
        count++;
    }

    if ( count == 0 ) fprintf ( stdout, "Unable to find any valid data\n" );

    fprintf ( stdout, "\n" );

    delete [] preamble;

    return count;
}

//----------------------------------------------------------------------------
//
//  In a perfect world, the recorded signal would be a nice square wave and
//  would be easy to decode.  The problem is an audio tape has a limitted
//  bandwidth and the signal is not a perfect square wave.  Worse, there is
//  often a DC drift, and variances in tape speed as well.  These conditions
//  make it difficult to easily decode the signal.  The ReadBit routine attempts
//  to compensate for these as best as possible.
//
//  To illustrate what is going on, the diagram below shows a sample sequence
//  of 0 0 0 1.  The first thing to notice is that each 'bit' does occupy the
//  same number of samples.  The third '0' bit shows the DC drift problem.  The
//  next zero crossing after 'C' should be at 'D', but there are two extra
//  crossings between them.
//
//  For a 3 MHz console:
//    frequency for '0' -  689.37 Hz (1450.6μs)
//    frequency for '1' - 1378.74 Hz ( 725.3μs)
//
//                 *                   *           *
//                 |                  * |          ||
//                * |                 | |         * |
//                | *                 | |       **  |
//                |  |                | *       |   *
//         * *** *   |               *   |     *     |
//        * *   *    |         * *** |   |     |     |
//    ---A-----------B---------C*---*----D-----E-----F----
//                   |        *          |    *      |
//                   * *   *  |          *   *       *
//                    * *** | |           |  |
//                          | |           |  |
//                          * |           *  |
//                           ||            |*
//                           *             *
//
//  Legend:
//     A - Start of 1st '0' bit
//     B - 1st bit boundary
//     C - 2nd bit boundary
//     D - 3rd bit boundary
//     E - '1' cell division
//     F - 4th bit boundary
//
//----------------------------------------------------------------------------

double period, minPer, maxPer, zero;
double threshold1 = 0.10;
double threshold2 = 0.80;

static int ReadBit ()
{
    FUNCTION_ENTRY ( NULL, "ReadBit", false );

    // Residual distance from last bit
    static double d0 = 0.0;

    double distance, d [16];

    distance = d [0] = d0;

    if ( verbose >= 3 ) fprintf ( stdout, "%08X d0=%10.5f", ( int ) ( tapeBuffer - tapeStart ), distance );

    // Keep reading until we have enough to fill a single bit cell
    int i = 0;
    while ( distance < minPer )
    {
        double dist = FindNextCrossing ();
        if ( dist < 0 ) return -1;
        distance += d [++i] = dist;
        if ( verbose >= 3 ) fprintf ( stdout, " %8.2f ", d [i] );
    }

    if ( verbose >= 3 ) fprintf ( stdout, " - %8.2f =>", distance );

    // If we went too far, remember how much we're over for the next time around
    d0 = ( distance > maxPer ) ? distance - maxPer : 0;

    // If we have way too much, we're probably not reading data and we don't want to carry this over
    if ( d0 > 2 * maxPer ) d0 = 0;

    // 9 times out of 10, this is caused by DC drift - call it a '0'
    if ( i == 3 ) return 0;

    if (( d0 >  0 ) && ( i == 2 )) return 0;

    for ( int x = 0; x <= i; x++ )
    {
        if ( d [x] >= zero )
        {
            return 0;
        }
    }

    return 1;
}

void AnalyzeBits ( const UINT8 *buffer, int length, double period )
{
    tapeBuffer = buffer;
    tapeStart  = buffer;
    tapeEnd    = tapeStart + length;

    int maxIndex = ( int ) ( 10 * ( period * 1.5 ));
    int *histogram = new int [ maxIndex ];
    memset ( histogram, 0, sizeof ( int ) * maxIndex );

    int max = 0;
    while ( tapeBuffer < tapeEnd )
    {
        int index = ( int ) ( FindNextCrossing () * 10.0 );
        if ( index < 0 ) break;
        if ( index >= maxIndex ) index = maxIndex - 1;
        histogram [index] += 1;
        if ( histogram [index] > max ) max = histogram [index];
    }

    int lo = 0;
    while ( histogram [lo] == 0 ) lo++;
    int hi = maxIndex - 1;
    while ( histogram [hi] == 0 ) hi--;

    for ( int i = lo; i <= hi; i++ )
    {
        printf ( "%4.1f: %5d |", i / 10.0, histogram [i] );
        int size = 132 * histogram [i] / max;
        for ( int x = 0; x < size; x++ )
        {
            printf ( "*" );
        }
        printf ( "\n" );
    }

    delete [] histogram;
}

static int FillBitBuffer ( const UINT8 *buffer, int length, double per, double minPeriod, double maxPeriod, UINT8 *bitBuffer )
{
    FUNCTION_ENTRY ( NULL, "FillBitBuffer", true );

    period = per;

    // Make sure we didn't get a bogus lower limit
    if ( minPeriod <= period / 2.0 )
    {
        minPeriod = period - ( maxPeriod - period );
    }

    minPer  = minPeriod * ( 1.0 - threshold1 );
    maxPer  = maxPeriod * ( 1.0 + threshold1 );
    zero = period * threshold2;
    if ( minPer < zero ) zero = minPer;

    // Set up the global pointer used by FindNextCrossing & GetSample routines
    tapeBuffer = buffer;
    tapeStart  = buffer;
    tapeEnd    = tapeStart + length;

    // Calculate the DC bias for this section of the tape
    bias = 0;
    for ( int i = 0; ; i++ )
    {
        bias += GetSample ();
        if ( tapeBuffer >= tapeEnd )
        {
            if ( i != 0 )
            {
                bias /= i;
            }
            break;
        }
    }

    // Reset the tape buffer
    tapeBuffer = buffer;

    if ( verbose >= 1 )
    {
        fprintf ( stdout, "\n" );
        fprintf ( stdout, "Settings:\n" );
        fprintf ( stdout, "  period       = %12.5f\n", period );
        fprintf ( stdout, "  minPer       = %12.5f\n", minPer );
        fprintf ( stdout, "  maxPer    = %12.5f\n", maxPer );
        fprintf ( stdout, "  bias        	 = %12.5f\n", bias );
        fprintf ( stdout, "\n" );
    }

    int count = 0;

ReadBit ();
buffer = tapeBuffer;

    if ( verbose == 2 ) fprintf ( stdout, "\n%08X: ", count );

    while ( tapeBuffer < tapeEnd )
    {
        if ( verbose >= 3 ) fprintf ( stdout, "\n%08X - ", count );

        int bit = ReadBit ();

        bitBuffer [count++] = ( UINT8 ) bit;

int samples = ( tapeBuffer - buffer ) / fmt.nBlockAlign;
//if ( fabs ( count * period - samples ) > 2.0 * period / 3.0 ) {
if ( fabs ( count * period - samples ) > 2 * maxPer )
{
    fprintf ( stdout, "Bit #%d - alignment slipped (%f | %f)\n", count, count * period - samples, ( double ) samples / period );
    buffer = tapeBuffer - ( int ) ( count * period ) * fmt.nBlockAlign;
}

        if ( verbose >= 2 )
        {
            switch ( bit )
            {
                case  0 : fprintf ( stdout, "0" );  break;
                case  1 : fprintf ( stdout, "1" );  break;
                case -1 : fprintf ( stdout, "@" );  break;
                case -2 : fprintf ( stdout, "#" );  break;
                case -3 : fprintf ( stdout, "*" );  break;
                case -4 : fprintf ( stdout, "?" );  break;
                case -5 : fprintf ( stdout, "+" );  break;
                default : fprintf ( stdout, "-" );  break;
            }

            if ( count % 8  == 0 ) fprintf ( stdout, ( verbose == 2 ) ? " " : "" );
            if ( count % 64 == 0 ) fprintf ( stdout, ( verbose == 2 ) ? "\n%08X: " : "", count );
        }
    }

    if ( verbose >= 1 )
    {
        if ( verbose >= 2 )
        {
            fprintf ( stdout, ( count % 64 == 0 ) ? "\n" : "\n\n" );
        }
        fprintf ( stdout, "%d bits read\n", count );
        fprintf ( stdout, "\n" );
    }

    bitBuffer [count] = ( UINT8 ) -1;

    return count;
}

//----------------------------------------------------------------------------
//
// Format of recorded data: Miller Code? Biphase Mark Code? - a simple Binary Line Code
//
//        Each bit cell contains either a '0' or '1'.  For a '0' value, the
//   recorded polarity remains constant for the duration of the bit cell.  A
//   '1' value is indicated by a transition at the mid-point of the bit cell.
//   The polarity is reversed at each cell boundary.
//
//   Dis/Fix 64
//
// |  Header   |Byte |  Preamble |       Data       |ck|
// |  >0300 |  |Count| 8 Bytes|  |     64 Bytes     |  |
//  00 -- 00 FF xx xx 00 -- 00 FF **** User Data *** yy
//                   [the data section is repeted twice]
//
//      xx - # of 64 byte blocks to follow
//      yy - 8-bit checksum
//
//----------------------------------------------------------------------------

const UINT8 *bitData;
const UINT8 *bitEnd;

inline int GetBit () { return ( bitData < bitEnd ) ? *bitData++ : -1; }

static const UINT8 *FindHeader ()
{
    FUNCTION_ENTRY ( NULL, "FindHeader", true );

    const UINT8 *start;

restart:

    start = bitData;

    // Read 0x0300 copies of 0x00
    for ( EVER )
    {
        int bit = GetBit ();
        if ( bit < 0 ) return NULL;
        if ( bit == 1 ) break;
    }

    // Look for 0xFF
    for ( int i = 1; i < 8; i++ )
    {
        int bit = GetBit ();
        if ( bit < 0 ) return NULL;
        if ( bit != 1 ) goto restart;
    }

    return start;
}

static int GetByte ()
{
    FUNCTION_ENTRY ( NULL, "GetByte", false );

    int byte = 0;
    for ( int i = 0; i < 8; i++ )
    {
        int bit = GetBit ();
        if ( bit < 0 ) return -1;
        byte = ( byte << 1 ) | bit;
    }

    return byte;
}

static const UINT8 *FindPreamble ( int length )
{
    FUNCTION_ENTRY ( NULL, "FindPreamble", true );

    const UINT8 *bitStart [128];
    memset ( bitStart, 0, sizeof ( bitStart ));

    DBG_ASSERT ( length >= 64 );

    int bit = 0, count = 0;

retry:

    // Look for 8 0x00's (we'll settle for 7)
    for ( EVER )
    {
        do
        {
            if ( count > length ) return NULL;
            bitStart [count % SIZE ( bitStart )] = bitData;
            bit = GetBit ();
            if ( bit < 0 ) return NULL;
            count++;
        }
        while ( bit == 0 );

        if ( count >= 56 ) break;

        // Ignore 'noise' at the beginning
        if ( count <= 8 ) count = 0;

    }

    // Look for 0xFF
    for ( int i = 1; i < 8; i++ )
    {
        if ( count > length ) return NULL;
        bitStart [count % SIZE ( bitStart )] = bitData;
        bit = GetBit ();
        if ( bit < 0 ) return NULL;
        if ( bit == 0 )
        {
            length         -= count;
            count        = 1;
            bitStart [0] = bitStart [count % SIZE ( bitStart )];
            goto retry;
        }
        count++;
    }

    if ( count > length ) return NULL;

    // Return the location of the 1st zero bit of the preamble
    return ( count >= 72 ) ? bitStart [count - 72 % SIZE ( bitStart )] : bitStart [0];
}

static void DumpSection ( const char *name, const UINT8 *ptr, int size )
{
    FUNCTION_ENTRY ( NULL, "DumpSection", true );

    if (( ptr == NULL ) || ( size < 0 ))
    {
        fprintf ( stdout, "%s: ----\n", name );
        return;
    }

    fprintf ( stdout, "%s: %4d - ", name, size );
    while ( size-- > 0 ) fprintf ( stdout, ( *ptr++ == 1 ) ? "1" : "0" );
    fprintf ( stdout, "\n" );
}

static bool ReadChunk ( const UINT8 *start, UINT8 *data )
{
    FUNCTION_ENTRY ( NULL, "ReadChunk", true );

    if ( start == NULL ) return false;

    bitData = start;

    int sum = 0;
    for ( int j = 0; j < 64; j++ )
    {
        int byte = GetByte ();
        if ( byte < 0 ) return false;
        sum += data [j] = ( UINT8 ) byte;
    }
    int actual = GetByte ();

    return (( sum & 0xFF ) == actual ) ? true : false;
}

const int MIN_RUN_SIZE = 1;

struct sRunInfo
{
    int start;
    int length;
    int shift;
};

struct sRunQueue
{
    int            count;
    sRunInfo       info [2048];
};

sRunQueue runQueue [2];

static void AddRun ( sRunQueue *queue, int start, int length, int shift )
{
    FUNCTION_ENTRY ( NULL, "AddRun", false );

    if ( start < 0 )
    {
        if ( start + length <= 0 ) return;
        length = start + length;
        start  = 0;
    }

    for ( int i = 0; i < queue->count; i++ )
    {
        sRunInfo *info = &queue->info [i];
        if (( start == info->start ) && ( length == info->length ))
        {
            if ( abs ( shift ) < abs ( info->shift )) info->shift = shift;
            return;
        }
    }

    if ( queue->count > ( int ) SIZE ( queue->info )) fprintf ( stdout, "QUEUE OVERFLOW!\n" );

    sRunInfo *info = &queue->info [queue->count++];
    info->start  = start;
    info->length = length;
    info->shift  = shift;
}

static int sortFunction1 ( const sRunInfo *p1, const sRunInfo *p2 )
{
    FUNCTION_ENTRY ( NULL, "sortFunction1", false );

    int delta;

    delta = p2->length - p1->length;
    if ( delta != 0 ) return delta;

    delta = p1->start - p2->start;
    if ( delta != 0 ) return delta;

    return abs ( p1->shift ) - abs ( p2->shift );
}

static int sortFunction2 ( const sRunInfo *p1, const sRunInfo *p2 )
{
    FUNCTION_ENTRY ( NULL, "sortFunction2", false );

    int delta;

    delta = p1->start - p2->start;
    if ( delta != 0 ) return delta;

    return abs ( p1->shift ) - abs ( p2->shift );
}

static void TrimRunQueue ( sRunQueue *queue )
{
    FUNCTION_ENTRY ( NULL, "TrimRunQueue", true );

    for ( int i = 0; i < queue->count; i++ )
    {
        sRunInfo *srcInfo = &queue->info [i];
        for ( int j = i + 1; j < queue->count; j++ )
        {
            sRunInfo *dstInfo = &queue->info [j];
            if (( srcInfo->start >= dstInfo->start ) && ( srcInfo->start <= dstInfo->start + dstInfo->length ))
            {
                dstInfo->length = srcInfo->start - dstInfo->start;
            }
            if (( dstInfo->start >= srcInfo->start ) && ( dstInfo->start <= srcInfo->start + srcInfo->length ))
            {
                dstInfo->length = dstInfo->start + dstInfo->length - ( srcInfo->start + srcInfo->length );
                dstInfo->start  = srcInfo->start + srcInfo->length;
            }
            if ( dstInfo->length <= 0 )
            {
                memcpy ( dstInfo, dstInfo + 1, sizeof ( sRunInfo ) * ( queue->count - j - 1 ));
                queue->count--;
                j--;
            }
        }
    }
}

//
// We have a (very poor) rate 1/2 code (actually a rate 32/65), so we might be able to recover from minor errors
//
static bool RecoverChunk ( const UINT8 *src1, int length1, const UINT8 *src2, int length2, UINT8 *data )
{
    FUNCTION_ENTRY ( NULL, "RecoverChunk", true );

    runQueue [0].count = 0;
    runQueue [1].count = 0;

    // We need to find out how much of the two copies match each other
    // By shifting them around, we may be able to detect/compensate for dropped bits
    for ( int offset = -8; offset < 8; offset++ )
    {
        const UINT8 *ptr1 = ( offset > 0 ) ? src1 + offset : src1;
        const UINT8 *ptr2 = ( offset < 0 ) ? src2 - offset : src2;

        int maxLength = ( offset < 0 ) ?
                  ( length2 + offset < length1 ) ? length2 + offset : length1 :
                  ( length1 - offset < length2 ) ? length1 - offset : length2;

        int matches = 0, run = 0;
        for ( int i = 0; i < maxLength; i++ )
        {
            if ( *ptr1++ == *ptr2++ )
            {
                matches++;
                run++;
            }
            else
            {
                int start = i - run;
                if ( run > MIN_RUN_SIZE )
                {
                    AddRun ( &runQueue [0], ( offset > 0 ) ? start - offset : start, run, offset );
                    AddRun ( &runQueue [1], ( offset < 0 ) ? start - offset : start, run, offset );
                }
                run = 0;
            }
        }

        int start = maxLength - run;
        AddRun ( &runQueue [0], ( offset > 0 ) ? start - offset : start, run, offset );
        AddRun ( &runQueue [1], ( offset < 0 ) ? start - offset : start, run, offset );

        // Look for the simple case - no dropped/added bits and less than a bytes worth of damage
        if (( maxLength >= 8 * ( 64 + 1 )) && ( maxLength - matches <= 8 ))
        {
            fprintf ( stdout, "-- possibly correctable errors detected with offset %d--\n", offset );
        }
    }

    qsort ( runQueue[0].info, runQueue[0].count, sizeof ( sRunInfo ), ( QSORT_FUNC ) sortFunction1 );
    TrimRunQueue ( &runQueue [0] );
    qsort ( runQueue[0].info, runQueue[0].count, sizeof ( sRunInfo ), ( QSORT_FUNC ) sortFunction2 );

    qsort ( runQueue[1].info, runQueue[1].count, sizeof ( sRunInfo ), ( QSORT_FUNC ) sortFunction1 );
    TrimRunQueue ( &runQueue [1] );
    qsort ( runQueue[1].info, runQueue[1].count, sizeof ( sRunInfo ), ( QSORT_FUNC ) sortFunction2 );

    if ( verbose >= 2 )
    {
        for ( int i = 0; i < 2; i++ )
        {
            fprintf ( stdout, "runQueue[%d]: %d entries\n", i, runQueue [i].count );
            int bits = 0;
            for ( int x = 0; x < runQueue[i].count; x++ )
            {
                fprintf ( stdout, " %3d - %3d, %3d\n", runQueue [i].info [x].start, runQueue [i].info [x].length, runQueue [i].info [x].shift );
                bits += runQueue [i].info[x].length;
            }
            fprintf ( stdout, "chunk[%d]: %3d good bits (short by %d)\n", i, bits, 8 * ( 64 + 1 ) - bits );
        }
    }

    return false;
}

static bool DumpBuffer ( const UINT8 *buffer, int size, const char *filename )
{
    FUNCTION_ENTRY ( NULL, "DumpBuffer", true );

    // Set up the globals and EOF condition
    bitData = buffer;
    bitEnd  = buffer + size;

    int nextSection;
    const UINT8 *tapeSection [1+2+256*2*2+1];
    memset ( tapeSection, 0, sizeof ( tapeSection ));

    // Find the start of the header
    tapeSection [0] = FindHeader ();
    if ( tapeSection [0] == NULL )
    {
        fprintf ( stdout, "Unable to locate start of data\n" );
        return false;
    }

    int headerSize = ( bitData - tapeSection [0] ) / 8;
    if ( headerSize < 3 * 0x300 / 5 )
    {
        fprintf ( stdout, "WARNING: Header is too short (%04X bytes)!\n", headerSize );
    }

    nextSection = 1;
    tapeSection [nextSection] = bitData;
    nextSection++;

    // Read count
    int count1 = GetByte ();
    int count2 = GetByte ();

    if ( count1 != count2 )
    {
        fprintf ( stdout, "Counts do not match! (%02X/%02X)\n", count1, count2 );
        return false;
    }

    if ( count1 <= 0 )
    {
        fprintf ( stdout, "Count is invalid! (%d)\n", count1 );
        return false;
    }

    // Given 'size' bits, calculate the number of chunks we should expect to see
    int bytesLeft = ( bitEnd - bitData ) / 8;
    int expected  = bytesLeft / ( 2 * ( 8 + 1 + 64 + 1 ));

    // Make sure the count is reasonably close to what we should expect
    if ( abs ( count1 - expected ) > 2 )
    {
        if ( count1 > expected )
        {
            fprintf ( stdout, "%d records indicated, but there is only enough data for %d records\n", count1, expected );
            return false;
        }
        fprintf ( stdout, "Found enough data for %d records - only %d records indicated\n", expected, count1 );
count1 = expected;
    }

    UINT8 *data = new UINT8 [ count1 * 64 ];

    bool good = true;

    int dataExpected = 0;

    for ( int i = 0; i < count1; i++ )
    {
        for ( int j = 0; j < 2; j++ )
        {
            // Find the preamble boundary
            const UINT8 *oldBitData = bitData;
            const int slop = 0; ////32;
            bitData += max ( dataExpected - slop, 0 );
            // Look for the preamble where we normally expect it to be
            const UINT8 *preamble = FindPreamble ( 8 * ( 8 + 1 ) + slop * 2 );
            if ( preamble == NULL )
            {
                // Extend the search - search the last chunk up through the next expected chunk
                bitData = oldBitData;
                preamble = FindPreamble ( dataExpected + 8 * ( 8 + 1 ) + 8 * ( 64 + 1 ) + slop );
                if ( preamble == NULL )
                {
                    // If it's not the 2nd copy of the last chunk of data give up
                    if (( i != count1 - 1 ) || ( j != 1 ))
                    {
                        fprintf ( stdout, "Unable to find the preamble for chunk %d/%d\n", i + 1, count1 );
                        good = false;
                        goto skip;
                    }
////                    preamble = bitData;
                }
            }
            // Record the start of the preamble and data
            tapeSection [nextSection++] = preamble;
            tapeSection [nextSection++] = bitData;
            // Assume we have at least 1/2 of the data intact
            dataExpected = 8 * ( 64 + 1 );
        }
    }

skip:

    tapeSection [nextSection] = bitEnd;

    if ( verbose >= 2 )
    {
        for ( int i = 0; i < count1; i++ )
        {
            int index = 2 + i * 4;
            fprintf ( stdout, "Chunk %3d\n", i );
            if ( index + 4 > nextSection )
            {
                fprintf ( stdout, "** Not enough valid data **\n" );
            }
            else
            {
                DumpSection ( "preamble[0]", tapeSection [index], tapeSection [index+1] - tapeSection [index] ); index++;
                DumpSection ( "    data[0]", tapeSection [index], tapeSection [index+1] - tapeSection [index] ); index++;
                DumpSection ( "preamble[1]", tapeSection [index], tapeSection [index+1] - tapeSection [index] ); index++;
                DumpSection ( "    data[1]", tapeSection [index], tapeSection [index+1] - tapeSection [index] ); index++;
            }
        }
    }

    if ( verbose >= 2 ) fprintf ( stdout, "\nstatus: " );

    for ( int i = 0; i < count1; i++ )
    {
        int index1 = 2 + 1 + i * 4;
        const UINT8 *src1 = tapeSection [index1];
        if ( ReadChunk ( src1, data + i * 64 ) == false )
        {
            int index2 = 2 + 1 + i * 4 + 2;
            const UINT8 *src2 = tapeSection [index2];
            if ( ReadChunk ( src2, data + i * 64 ) == false )
            {
                int length1 = tapeSection [index1+1] - tapeSection [index1];
                int length2 = tapeSection [index2+1] - tapeSection [index2];
                if ( RecoverChunk ( src1, length1, src2, length2, data + i * 64 ) == false )
                {
                    good = false;
                    fprintf ( stdout, "x" );
                    continue;
                }
                fprintf ( stdout, "*" );
                continue;
            }
        }
        fprintf ( stdout, "." );
    }

    fprintf ( stdout, "\n" );

    if ( good == true )
    {
        FILE *file = fopen ( filename, "wb" );
        fwrite ( data, count1, 64, file );
        fclose ( file );
    }

    delete [] data;

    return good;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: decode [options] file.wav\n" );
    fprintf ( stdout, "\n" );
}

int main ( int argc, char *argv [] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    sOption optList [] =
    {
        { 'v', "verbose*=n",                   OPT_VALUE_PARSE_INT, 1,      &verbose,  NULL,                    "Display extra information" }
    };

    if ( argc == 1 )
    {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    fprintf ( stdout, "TI-99/4A Cassette Utility\n\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc )
    {
        fprintf ( stdout, "No input file specified\n" );
        return -1;
    }

    char fileName [256];
    strcpy ( fileName, argv [index] );

    FILE *file = fopen ( fileName, "rb" );
    if ( file == NULL )
    {
        strcat ( fileName, ".wav" );
        file = fopen ( fileName, "rb" );
        if ( file == NULL )
        {
            fprintf ( stdout, "Unable to open file \"%s\"\n", argv [index] );
            return -1;
        }
    }

    fprintf ( stdout, "File \"%s\"\n", fileName );

    fseek ( file, 0, SEEK_END );
    UINT32 size = ftell ( file );
    fseek ( file, 0, SEEK_SET );

    UINT8 *buffer = new UINT8 [ size ];
    UINT32 bytesLeft = size;

    UINT8 *pData;
    int    DataSize;

    bytesLeft -= fread ( buffer, 1, ( bytesLeft < 512 ) ? bytesLeft : 512, file );

    if ( CheckHeader ( buffer, 512, &pData, &DataSize, &fmt ) == false )
    {
        fprintf ( stdout, "Invalid WAV file\n" );
        delete [] buffer;
        fclose ( file );
        return -1;
    }

    if ( fmt.wFormatTag != WAVE_FORMAT_PCM )
    {
        fprintf ( stdout, "Unsupported WAVE format\n" );
        delete [] buffer;
        fclose ( file );
        return -1;
    }

    // Make sure bits/sample is a multiple of 8
    fmt.wBitsPerSample = ( fmt.wBitsPerSample + 7 ) & 0xF8;
    if ( fmt.wBitsPerSample == 0 )
    {
        fmt.wBitsPerSample = ( UINT16 ) ( 8 * fmt.nAvgBytesPerSec / fmt.nSamplesPerSec / fmt.nChannels );
    }

    if ( fmt.wValidBitsPerSample == 0 )
    {
        fmt.wValidBitsPerSample = fmt.wBitsPerSample;
    }

    switch ( fmt.nChannels )
    {
        case 1  : GetSample = ( fmt.wBitsPerSample == 8 ) ? GetSampleMono8   : ( fmt.wBitsPerSample == 16 ) ? GetSampleMono16   : GetSampleMono32;    break;
        default : GetSample = ( fmt.wBitsPerSample == 8 ) ? GetSampleStereo8 : ( fmt.wBitsPerSample == 16 ) ? GetSampleStereo16 : GetSampleStereo32;  break;
    }

    fprintf ( stdout, "Format: %d-bit %s %d,%03dHz\n", fmt.wValidBitsPerSample,
                                                       ( fmt.nChannels == 1 ) ? "Mono" :
                                                       ( fmt.nChannels == 2 ) ? "Stereo" : "Multi-track",
                                                       fmt.nSamplesPerSec / 1000, fmt.nSamplesPerSec % 1000 );

    fprintf ( stdout, "\n" );

    fprintf ( stdout, "Reading file " );
    const UINT32 CHUNK_SIZE = 512 * 1024;
    for ( UINT32 i = 512; i < size; i += CHUNK_SIZE )
    {
        fprintf ( stdout, "." );
        fflush ( stdout );
        bytesLeft -= fread ( buffer + i, 1, ( bytesLeft < CHUNK_SIZE ) ? bytesLeft : CHUNK_SIZE, file );
    }
    fclose ( file );
    fprintf ( stdout, "\n" );

    fprintf ( stdout, "\n" );

    sFileData fileData [32];
    memset ( fileData, 0, sizeof ( fileData ));

    // Locate individual files within the data stream
    int count = LocateTracks ( pData, DataSize, fileData );

    // Try to extract each file
    for ( int i = 0; i < count; i++ )
    {
        char filename [ 256 ];
        sprintf ( filename, "track-%02d.dat", i + 1 );
        fprintf ( stdout, "Reading track %2d: ", i + 1 );

        UINT8 *bitBuffer = new UINT8 [ fileData [i].dataLength ];

        int bitCount = FillBitBuffer ( fileData [i].pData, fileData [i].dataLength, fileData [i].period, fileData [i].minPeriod, fileData [i].maxPeriod, bitBuffer );
        if ( DumpBuffer ( bitBuffer, bitCount, filename ) != true )
        {
            fprintf ( stdout, "Unable to recover data on track %d\n", i + 1 );
        }

        delete [] bitBuffer;
    }

    delete [] buffer;

    return 0;
}
