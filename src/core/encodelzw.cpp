//----------------------------------------------------------------------------
//
// File:        encodelzw.cpp
// Date:        22-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to create a compressed ARK file (Barry Boone's Archive format)
//
// Copyright (c) 2003-2004 Marc Rousseau, All Rights Reserved.
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

#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "encodelzw.hpp"

DBG_REGISTER ( __FILE__ );

using namespace encode;

cEncodeLZW::cEncodeLZW ( int bits ) :
    m_MAX_BITS ( bits ),
    m_CODE_MAX ( 1 << bits ),
    m_nBits ( 0 ),
    m_BitOff ( 0 ),
    m_CurWord ( 0 ),
    m_MaxCode ( 0 ),
    m_AppendChar ( NULL ),
    m_FirstHash ( NULL ),
    m_NextHash ( NULL ),
    m_FreeCode ( 0 ),
    m_HashEmpty ( true ),
    m_InPtr ( NULL ),
    m_InEnd ( NULL ),
    m_WriteCallback ( NULL ),
    m_CallbackToken ( NULL ),
    m_MaxWriteSize ( 0 ),
    m_WriteBuffer ( NULL ),
    m_OutPtr ( NULL )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW ctor", true );

    m_AppendChar = new UINT8 [ m_CODE_MAX ];
    m_FirstHash  = new UINT16 [ m_CODE_MAX ];
    m_NextHash   = new UINT16 [ m_CODE_MAX ];

    memset ( m_AppendChar, 0, sizeof ( UINT8 ) * m_CODE_MAX );
    memset ( m_FirstHash, 0, sizeof ( UINT16 ) * m_CODE_MAX );
    memset ( m_NextHash, 0, sizeof ( UINT16 ) * m_CODE_MAX );
}

cEncodeLZW::~cEncodeLZW ()
{
    FUNCTION_ENTRY ( this, "cEncodeLZW dtor", true );

    delete [] m_NextHash;
    delete [] m_FirstHash;
    delete [] m_AppendChar;
}

void cEncodeLZW::InitializeTable ()
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::InitializeTable", true );

    m_nBits    = 9;
    m_MaxCode  = ( 1 << m_nBits );

    for ( int i = 0; i < 256; i++ ) {
        m_FirstHash [i] = END_PATTERN;
        m_NextHash [i]  = END_PATTERN;
    }

    m_FreeCode = CODE_FIRST_FREE;
}

UINT8 cEncodeLZW::ReadChar ()
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::ReadChar", true );

    if ( m_InPtr >= m_InEnd ) throw 1;

    return *m_InPtr++;
}

void cEncodeLZW::FlushOutput ( bool force )
{
    int bytes = m_BitOff / 8;
    int shift = ( bytes - 1 ) * 8;

    m_BitOff %= 8;

    UINT32 data = m_CurWord >> m_BitOff;

    while ( bytes-- > 0 ) {
        *m_OutPtr++ = ( data >> shift ) & 0xFF;
        shift    -= 8;
    }

    if (( force == true ) || ( m_OutPtr >= m_WriteBuffer + m_MaxWriteSize )) {
        if (( force == true ) && ( m_BitOff > 0 )) {
            *m_OutPtr++ = ( m_CurWord << ( 8 - m_BitOff )) & 0xFF;
        }
        size_t size = m_OutPtr - m_WriteBuffer;
        m_OutPtr    = m_WriteBuffer;
        m_WriteCallback ( m_WriteBuffer, size, m_CallbackToken );
    }
}

bool cEncodeLZW::WriteCode ( UINT16 code )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::WriteCode", true );

    m_CurWord = ( m_CurWord << m_nBits ) | code;
    m_BitOff += m_nBits;

    FlushOutput ( false );

    return true;
}

bool cEncodeLZW::Lookup ( UINT16 *prefix, UINT16 ch, UINT16 *lastIndex )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::Lookup", true );

    m_HashEmpty = true;
    *lastIndex  = *prefix;

    UINT16 index = m_FirstHash [*lastIndex];
    if ( index != END_PATTERN ) {
        m_HashEmpty = false;
        do {
            if ( m_AppendChar [index] == ch ) {
                *prefix = index;
                return true;
            }
            *lastIndex = index;
            index = m_NextHash [index];
        } while ( index != END_PATTERN );
    }

    return false;
}

UINT16 cEncodeLZW::AddCode ( UINT16 index, UINT16 ch )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::AddCode", true );

    index [ m_HashEmpty ? m_FirstHash : m_NextHash ] = m_FreeCode;

    if ( m_FreeCode != m_CODE_MAX ) {
        m_FirstHash [m_FreeCode]  = END_PATTERN;
        m_NextHash [m_FreeCode]   = END_PATTERN;
        m_AppendChar [m_FreeCode] = ch;
        return m_FreeCode++;
    } else {
        return m_FreeCode;
    }
}

void cEncodeLZW::SetWriteCallback ( bool (*callback) ( void *, size_t, void * ), void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::SetWriteCallback", true );

    m_WriteCallback = callback;
    m_CallbackToken = token;
    m_MaxWriteSize  = size;

    m_WriteBuffer = ( UINT8 * ) buffer;
    m_OutPtr      = m_WriteBuffer;
}

int cEncodeLZW::EncodeBuffer ( void *buffer, size_t size )
{
    FUNCTION_ENTRY ( this, "cEncodeLZW::EncodeBuffer", true );

    if ( size == 0 ) return -1;

    m_BitOff  = 0;
    m_CurWord = 0;

    InitializeTable ();

    m_InPtr = ( UINT8 * ) buffer;
    m_InEnd = ( UINT8 * ) buffer + size;

    WriteCode ( CODE_CLEAR );

    UINT16 prefix = ReadChar ();

    try {

        for ( EVER ) {

            UINT16 ch, lastIndex;

            do {
                ch = ReadChar ();
            } while ( Lookup ( &prefix, ch, &lastIndex ) == true );

            WriteCode ( prefix );

            prefix = ch;

            if ( AddCode ( lastIndex, ch ) >= m_MaxCode ) {
                if ( m_nBits == m_MAX_BITS ) {
                    WriteCode ( CODE_CLEAR );
                    InitializeTable ();
                } else {
                    m_nBits++;
                    m_MaxCode <<= 1;
                }
            }
        }
    }

    catch ( ... ) {

        WriteCode ( prefix );
        WriteCode ( CODE_EOF );

        FlushOutput ( true );
    }

    return 1;
}
