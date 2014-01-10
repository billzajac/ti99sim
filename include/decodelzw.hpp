//----------------------------------------------------------------------------
//
// File:        decodelzw.hpp
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

#ifndef DECODELZW_HPP_
#define DECODELZW_HPP_

namespace decode {

    const int CODE_CLEAR      = 256;    // clear code
    const int CODE_EOF        = 257;    // CODE_EOF marker
    const int CODE_FIRST_FREE = 258;    // first free code
    const int CODE_ERROR      = ( UINT16 ) -1;

}

class cDecodeLZW {

    int     m_MAX_BITS;
    int     m_CODE_MAX;

    // Used for reading the next token
    int     m_nBits;
    int     m_BitOff;
    UINT32  m_CurWord;
    UINT16  m_CodeMask;

    // History tables
    UINT8  *m_AppendChar;
    UINT16 *m_NextCode;
    UINT16  m_FreeCode;

    // Input buffer related
    UINT8   *m_InPtr;
    UINT8   *m_InEnd;

    // Output buffer related
    bool   (*m_WriteCallback) ( void *, size_t, void * );
    void    *m_CallbackToken;
    size_t   m_MaxWriteSize;
    UINT8   *m_WriteBuffer;
    UINT8   *m_OutPtr;

    void Init ();
    void Reset ();
    void Done ();

    UINT16 ReadCode ();

    void AddCode ( UINT16, UINT8 );
    void WriteChar ( UINT8 ch );

public:

    cDecodeLZW ( int );
    ~cDecodeLZW ();

    int  BytesLeft () const;
    void SetWriteCallback ( bool (*) ( void *, size_t, void * ), void *, size_t, void * );
    int  ParseBuffer ( void *, size_t );

private:

    cDecodeLZW ( const cDecodeLZW & );      // no implementation
    void operator = ( const cDecodeLZW & ); // no implementation

};

#endif
