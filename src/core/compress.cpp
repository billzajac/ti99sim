//----------------------------------------------------------------------------
//
// File:        compress.cpp
// Date:        27-Mar-1998
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
#include "common.hpp"
#include "logger.hpp"

DBG_REGISTER ( __FILE__ );

#define MIN_RUN		4

static int GetRunLength ( int bytesLeft, const UINT8 *ptr, UINT8 lastChar )
{
    FUNCTION_ENTRY ( NULL, "GetRunLength", true );

    int runLength = 0;
    while ( bytesLeft && ( *ptr++ == lastChar )) {
        if ( ++runLength >= 0x7FFF ) break;
        bytesLeft--;
    }
    return runLength;
}

bool SaveBuffer ( int length, const UINT8 *ptr, FILE *file )
{
    FUNCTION_ENTRY ( NULL, "SaveBuffer", true );

    while ( length ) {
        UINT16 tag, count;
        int runLength = GetRunLength ( length, ptr, *ptr );
        if ( runLength >= MIN_RUN ) {
            tag = ( UINT16 ) ( runLength | 0x8000 );
            count = 1;
        } else {
            int bytesLeft = length - runLength;
            UINT8 lastChar = *ptr;
            const UINT8 *nextPtr = ptr + runLength;
            while ( bytesLeft ) {
                while ( bytesLeft && ( *nextPtr != lastChar )) {
                    bytesLeft--;
                    lastChar = *nextPtr++;
                    if ( ++runLength >= 0x7FFF ) break;
                }
                if ( runLength >= 0x7FFF ) break;
                if ( bytesLeft ) {
                    int tempRun = GetRunLength ( bytesLeft, nextPtr, *nextPtr );
                    if ( tempRun >= MIN_RUN ) break;
                    runLength += tempRun;
                    if ( runLength >= 0x7FFF ) {
                        runLength = 0x7FFF;
                        break;
                    }
                    nextPtr   += tempRun;
                    bytesLeft -= tempRun;
                }
            }
            tag = count = ( UINT16 ) runLength;
        }
        fputc ( tag, file );
        fputc ( tag >> 8, file );
        if ( fwrite ( ptr, 1, count, file ) != count ) {
            return false;
        }
        ptr    += runLength;
        length -= runLength;
    }

    return true;
}

bool LoadBuffer ( int length, UINT8 *ptr, FILE *file )
{
    FUNCTION_ENTRY ( NULL, "LoadBuffer", true );

    while ( length > 0 ) {
        UINT16 tag = ( UINT16 ) fgetc ( file );
        tag |= ( UINT8 ) fgetc ( file ) << 8;
        DBG_ASSERT (( tag & 0x7FFF ) <= length );
        if ( tag & 0x8000 ) {
            UINT8 runChar;
            if ( fread ( &runChar, 1, 1, file ) != 1 ) {
                return false;
            }
            int count = tag & 0x7FFF;
            length -= count;
            while ( count-- ) *ptr++ = runChar;
        } else {
            if ( tag == 0 ) {
                DBG_ERROR ( "Invalid compressed buffer" );
                return false;
            }
            if ( fread ( ptr, 1, tag, file ) != tag ) {
                return false;
            }
            ptr += tag;
            length -= tag;
        }
    }

    return true;
}
