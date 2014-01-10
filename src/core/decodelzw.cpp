//----------------------------------------------------------------------------
//
// File:        decodelzw.cpp
// Date:        15-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to decode an LZW compressed ARK file (Barry Boone's Archive format)
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
#include "decodelzw.hpp"

DBG_REGISTER ( __FILE__ );

using namespace decode;

const int MAX_STACK_SIZE = 1 << 15;

cDecodeLZW::cDecodeLZW ( int bits ) :
    m_MAX_BITS ( bits ),
    m_CODE_MAX ( 1 << bits ),
    m_nBits ( 0 ),
    m_BitOff ( 0 ),
    m_CurWord ( 0 ),
    m_CodeMask ( 0 ),
    m_AppendChar ( NULL ),
    m_NextCode ( NULL ),
    m_FreeCode ( 0 ),
    m_InPtr ( NULL ),
    m_InEnd ( NULL ),
    m_WriteCallback ( NULL ),
    m_CallbackToken ( NULL ),
    m_MaxWriteSize ( 0 ),
    m_WriteBuffer ( NULL ),
    m_OutPtr ( NULL )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW ctor", true );

    m_AppendChar = new UINT8 [ m_CODE_MAX ];
    m_NextCode   = new UINT16 [ m_CODE_MAX ];

    memset ( m_AppendChar, 0, sizeof ( UINT8 ) * m_CODE_MAX );
    memset ( m_NextCode, 0, sizeof ( UINT16 ) * m_CODE_MAX );
}

cDecodeLZW::~cDecodeLZW ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW dtor", true );

    delete [] m_NextCode;
    delete [] m_AppendChar;

    m_InPtr         = NULL;
    m_InEnd         = NULL;
    m_WriteCallback = NULL;
    m_CallbackToken = NULL;
    m_WriteBuffer   = NULL;
    m_OutPtr        = NULL;
}

void cDecodeLZW::Reset ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Reset", true );

    m_nBits    = 9;
    m_CodeMask = ( 1 << m_nBits ) - 1;
    m_FreeCode = CODE_FIRST_FREE;
}

void cDecodeLZW::Init ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Init", true );

    Reset ();
    m_BitOff = 0;
}

void cDecodeLZW::WriteChar ( UINT8 ch )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::WriteChar", false );

    DBG_ASSERT ( m_OutPtr != NULL );

    *m_OutPtr++ = ch;

    if ( m_OutPtr == m_WriteBuffer + m_MaxWriteSize ) {
        if ( m_WriteCallback ( m_WriteBuffer, m_MaxWriteSize, m_CallbackToken ) == false ) throw 0;
        m_OutPtr = m_WriteBuffer;
    }
}

void cDecodeLZW::Done ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::Done", true );

    if ( m_OutPtr != m_WriteBuffer ) {
        if ( m_WriteCallback ( m_WriteBuffer, m_OutPtr - m_WriteBuffer, m_CallbackToken ) == false ) throw 0;
        m_OutPtr = m_WriteBuffer;
    }
}

UINT16 cDecodeLZW::ReadCode ()
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::ReadCode", false );

    while ( m_BitOff < m_nBits ) {
        if ( m_InPtr >= m_InEnd ) throw -1;
        m_CurWord = ( m_CurWord << 8 ) | *m_InPtr++;
        m_BitOff += 8;
    }

    m_BitOff -= m_nBits;

    return ( m_CurWord >> m_BitOff ) & m_CodeMask;
}

void cDecodeLZW::AddCode ( UINT16 prefix, UINT8 ch )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::AddCode", false );

    if ( m_FreeCode >= m_CODE_MAX ) throw -2;

    UINT16 index         = m_FreeCode++;
    m_AppendChar [index] = ch;
    m_NextCode [index]   = prefix;
}

int cDecodeLZW::BytesLeft () const
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::BytesLeft", true );

    return m_InEnd - m_InPtr;
}

void cDecodeLZW::SetWriteCallback ( bool (*callback) ( void *, size_t, void * ), void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::SetWriteCallback", true );

    m_WriteCallback = callback;
    m_CallbackToken = token;
    m_MaxWriteSize  = size;

    m_WriteBuffer = ( UINT8 * ) buffer;
    m_OutPtr      = m_WriteBuffer;
}

int cDecodeLZW::ParseBuffer ( void *buffer, size_t size )
{
    FUNCTION_ENTRY ( this, "cDecodeLZW::ParseBuffer", true );

    UINT8   stackBuffer [ MAX_STACK_SIZE ];
    UINT8  *stack = stackBuffer + m_CODE_MAX;

    m_InPtr = ( UINT8 * ) buffer;
    m_InEnd = ( UINT8 * ) buffer + size;

    Init ();

    try {

        UINT8   ch     = 0;
        UINT16  prefix = 0;

        for ( EVER ) {

            UINT16 code = ReadCode ();

            switch ( code ) {

                case CODE_EOF :

                    Done ();
                    return 1;

                case CODE_CLEAR :

                    Reset ();
                    WriteChar ( ch = prefix = ReadCode ());
                    break;

                default: {

                    UINT16 index = code;

                    if ( code >= m_FreeCode ) {
                        if ( code > m_FreeCode ) {
                            DBG_ERROR ( "Archive is invalid" );
                            return -1;
                        }
                        index  = prefix;
                        *--stack = ch;
                    }

                    while ( index > 0xFF ) {
                        *--stack = m_AppendChar [index];
                        index  = m_NextCode [index];
                    }

                    ch = index;

                    WriteChar ( index );
                    while ( stack != stackBuffer + m_CODE_MAX ) {
                        WriteChar ( *stack++ );
                    }

                    AddCode ( prefix, ch );
                    prefix = code;

                    if (( m_FreeCode > m_CodeMask ) && ( m_nBits != m_MAX_BITS )) {
                        m_nBits++;
                        m_CodeMask = ( m_CodeMask << 1 ) | 1;
                    }
                    break;
                }
            }
        }
    }

    catch ( int retVal ) {
        return retVal;
    }

    return -1;
}
