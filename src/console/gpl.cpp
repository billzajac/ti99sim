//----------------------------------------------------------------------------
//
// File:        gpl.cpp
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

#include <stdio.h>
#include <string.h>
#include "common.hpp"

// Bit masks for Format 6 opcodes
const int MASK_NUMBER_IS_IMMEDIATE  = 0x01;
const int MASK_SOURCE_IS_GROM       = 0x02;
const int MASK_SOURCE_IS_VDP_OR_CPU = 0x04;
const int MASK_DEST_IS_VDP_REGISTER = 0x08;
const int MASK_DEST_IS_NOT_GROM     = 0x10;
const int MASK_OPCODE               = 0xE0;

const UINT8 *InterpretFormat0 ( const UINT8 *opcode );
const UINT8 *InterpretFormat1 ( const UINT8 *opcode );
const UINT8 *InterpretFormat2Byte ( const UINT8 *opcode );
const UINT8 *InterpretFormat2Word ( const UINT8 *opcode );
const UINT8 *InterpretFormat3 ( const UINT8 *opcode );
const UINT8 *InterpretFormat4 ( const UINT8 *opcode );
const UINT8 *InterpretFormat5 ( const UINT8 *opcode );
const UINT8 *InterpretFormat6 ( const UINT8 *opcode );
const UINT8 *InterpretFormat7 ( const UINT8 *opcode );

typedef const UINT8 *(*gplFunction) ( const UINT8 * );

struct gplCode {
    char        mnemonic [6];
    UINT8       lowOpCode;
    UINT8       highOpCode;
    int         Format;
    gplFunction Function;
};

static UINT16 base;
static int    isWord;
static char   argBuffer [ 80 ];

/*
    MSB 0 1 2 3 4 5 6 7 LSB

   1    0 Address         CPU RAM is directly addressed >8300 - >837F
   2    1 0 V I Address   V > 0 = CPU RAM  - 1 = VDP RAM
        Address           I > 0 = Direct   - 1 = Indirect
   3    1 1 V I Address   Same as #2, but indexed
        Address
        Index
   4    1 0 V I 1 1 1 1   Extended area at 0 through 65535. Address with
        Address           offset >8300, ie: >DD00 corresponds to address >6000
        Address
   5    1 1 V I 1 1 1 1   Link number 4, but indexed
        Address
        Address
        Index

<S/D>
    IMM		Immediate value					BACK  >20
    @IMM	Direct RAM/ROM					CLR   @>8300
    *IMM	Indirect to/from RAM/ROM into RAM/ROM		CZ    *FAC
    V@IMM	VPD Direct
    V*IMM	VDP Indirect by RAM/ROM
    @IMM(@SP)	Direct in RAM/ROM, indexed by scratchpad value
    *IMM(@SP)	Indirect in RAM/ROM, indexed by scratchpad value
    V@IMM(@SP)	Like @IMM(@SP), but the result is in VDP memory
    V*IMM(@SP)	Like *IMM(@SP), but the result is in VDP memory
-----------------------------
<MOVES>
    G@IMM	Direct GROM/GRAM
    G@IMM(@SP)	Like @IMM(@SP), but the result is in GROM memory
-----------------------------
<MOVED>
    #IMM	VDP register direct ( IMM = 0 to 7 )
*/

gplCode GPL [] = {
  { "RTN  ", 0x00, 0x00, 3, InterpretFormat3 },
  { "RTNC ", 0x01, 0x01, 3, InterpretFormat3 },
  { "RAND ", 0x02, 0x02, 2, InterpretFormat2Byte },
  { "SCAN ", 0x03, 0x03, 3, InterpretFormat3 },
  { "BACK ", 0x04, 0x04, 2, InterpretFormat2Byte },
  { "B    ", 0x05, 0x05, 2, InterpretFormat2Word },
  { "CALL ", 0x06, 0x06, 2, InterpretFormat2Word },
  { "ALL  ", 0x07, 0x07, 2, InterpretFormat2Byte },
  { "FMT  ", 0x08, 0x08, 7, InterpretFormat7 },
  { "H    ", 0x09, 0x09, 3, InterpretFormat3 },
  { "GT   ", 0x0A, 0x0A, 3, InterpretFormat3 },
  { "EXIT ", 0x0B, 0x0B, 3, InterpretFormat3 },
  { "CARRY", 0x0C, 0x0C, 3, InterpretFormat3 },
  { "OVF  ", 0x0D, 0x0D, 3, InterpretFormat3 },
  { "PARSE", 0x0E, 0x0E, 2, InterpretFormat2Byte },
  { "XML  ", 0x0F, 0x0F, 2, InterpretFormat2Byte },
  { "CONT ", 0x10, 0x10, 3, InterpretFormat3 },
  { "EXEC ", 0x11, 0x11, 3, InterpretFormat3 },
  { "RTNB ", 0x12, 0x12, 3, InterpretFormat3 },
  { "RTGR ", 0x13, 0x13, 3, InterpretFormat3 },
// 14-1F - XGPL
  { "MOVE ", 0x20, 0x3F, 6, InterpretFormat6 },
  { "BR   ", 0x40, 0x5F, 4, InterpretFormat4 },
  { "BS   ", 0x60, 0x7F, 4, InterpretFormat4 },
  { "ABS  ", 0x80, 0x81, 5, InterpretFormat5 },
  { "NEG  ", 0x82, 0x83, 5, InterpretFormat5 },
  { "INV  ", 0x84, 0x85, 5, InterpretFormat5 },
  { "CLR  ", 0x86, 0x87, 5, InterpretFormat5 },
  { "FETCH", 0x88, 0x89, 5, InterpretFormat5 },
  { "CASE ", 0x8A, 0x8B, 5, InterpretFormat5 },
  { "PUSH ", 0x8C, 0x8D, 5, InterpretFormat5 },
  { "CZ   ", 0x8E, 0x8F, 5, InterpretFormat5 },
  { "INC  ", 0x90, 0x91, 5, InterpretFormat5 },
  { "DEC  ", 0x92, 0x93, 5, InterpretFormat5 },
  { "INCT ", 0x94, 0x95, 5, InterpretFormat5 },
  { "DECT ", 0x96, 0x97, 5, InterpretFormat5 },
// 98-9F - XGPL
  { "ADD  ", 0xA0, 0xA3, 1, InterpretFormat1 },
  { "SUB  ", 0xA4, 0xA7, 1, InterpretFormat1 },
  { "MUL  ", 0xA8, 0xAB, 1, InterpretFormat1 },
  { "DIV  ", 0xAC, 0xAF, 1, InterpretFormat1 },		// AC-AF - ???
  { "AND  ", 0xB0, 0xB3, 1, InterpretFormat1 },
  { "OR   ", 0xB4, 0xB7, 1, InterpretFormat1 },
  { "XOR  ", 0xB8, 0xBB, 1, InterpretFormat1 },
  { "ST   ", 0xBC, 0xBF, 1, InterpretFormat1 },
  { "EX   ", 0xC0, 0xC3, 1, InterpretFormat1 },
  { "CH   ", 0xC4, 0xC7, 1, InterpretFormat1 },
  { "CHE  ", 0xC8, 0xCB, 1, InterpretFormat1 },
  { "CGT  ", 0xCC, 0xCF, 1, InterpretFormat1 },
  { "CGE  ", 0xD0, 0xD3, 1, InterpretFormat1 },
  { "CEQ  ", 0xD4, 0xD7, 1, InterpretFormat1 },
  { "CLOG ", 0xD8, 0xDB, 1, InterpretFormat1 },
  { "SRA  ", 0xDC, 0xDF, 1, InterpretFormat1 },
  { "SLL  ", 0xE0, 0xE3, 1, InterpretFormat1 },
  { "SRL  ", 0xE4, 0xE7, 1, InterpretFormat1 },
  { "SRC  ", 0xE8, 0xEB, 1, InterpretFormat1 },
// EC    - ????
  { "COINC", 0xED, 0xED, 1, InterpretFormat1 },
// EE-EF - ????
// F0-F3 - XGPL
  { "I/O  ", 0xF4, 0xF7, 1, InterpretFormat1 },
  { "SWGR ", 0xF8, 0xFB, 1, InterpretFormat1 },
// FC-FF XGPL
  { "XGPL ", 0x14, 0x1F, 0, InterpretFormat0 },
  { "XGPL ", 0x98, 0x9F, 0, InterpretFormat0 },
  { "XGPL ", 0xF0, 0xF3, 0, InterpretFormat0 },
  { "XGPL ", 0xFC, 0xFF, 0, InterpretFormat0 },

  { "UNKN ", 0x00, 0xFF, -1, NULL }
};

char *GetSourceDestination ( const UINT8 *&opcode )
{
    static char tempBuffer [ 20 ];
    char *ptr = tempBuffer;
    UINT8 flags = *opcode++;

    if (( flags & 0x80 ) == 0 ) {
        sprintf ( ptr, "@>%04X", 0x8300 + flags );
    } else {
        UINT16 address = (( flags << 8 ) | *opcode++ ) & 0x0FFF;

        if ( address >= 0x0F00 ) {		// Extended address
            address = (( address << 8 ) | *opcode++ );
        }
        if ( flags & 0x20 ) {
            *ptr++ = 'V';
            if ( flags & 0x10 ) {
                address += 0x8300;
            }
        } else {
            address += 0x8300;
        }
        *ptr++ = ( flags & 0x10 ) ? '*' : '@';
        ptr += sprintf ( ptr, ">%04X", address );
        if ( flags & 0x40 ) {
            sprintf ( ptr, "(@>%04X)", 0x8300 + *opcode++ );
        }
    }
    return tempBuffer;
}

char *GetArgument ( UINT8 mode, const UINT8 *&opcode )
{
    static char tempBuffer [ 20 ];

    if ( mode & 0x80 ) {
        return GetSourceDestination ( opcode );
    } else {
        char *ptr = tempBuffer;
        ptr += sprintf ( ptr, "G@>%04X", ( opcode[0] << 8 ) | opcode[1] );
        opcode += 2;
        if ( mode & 0x40 ) {
            sprintf ( ptr, "(@>%04X)", 0x8300 + *opcode++ );
        }
    }
    return tempBuffer;
}

const UINT8 *InterpretFormat0 ( const UINT8 *opcode )
{
    // Undocumented - XGPL
    return opcode + 1;
}

const UINT8 *InterpretFormat1 ( const UINT8 *opcode )
{
    int Flags = *opcode++;
    isWord = Flags & 0x01;
    char temp [ 20 ];
    sprintf ( temp, ",%s", GetSourceDestination ( opcode ));
    if ( Flags & 0x02 ) {
        int i = sprintf ( argBuffer, ">%02X", *opcode++ );
        if ( isWord ) {
            sprintf ( &argBuffer[i], "%02X", *opcode++ );
        }
    } else {
        strcpy ( argBuffer, GetSourceDestination ( opcode ));
    }
    strcat ( argBuffer, temp );
    return opcode;
}

const UINT8 *InterpretFormat2Byte ( const UINT8 *opcode )
{
    sprintf ( argBuffer, ">%02X", opcode[1] );
    return opcode + 2;
}

const UINT8 *InterpretFormat2Word ( const UINT8 *opcode )
{
    int imm = ( opcode[1] << 8 ) | opcode[2];
    sprintf ( argBuffer, ">%04X", imm );
    return opcode + 3;
}

const UINT8 *InterpretFormat3 ( const UINT8 *opcode )
{
    // no arguments
    return opcode + 1;
}

const UINT8 *InterpretFormat4 ( const UINT8 *opcode )
{
    int imm = ( opcode[0] << 8 ) | opcode[1];
    sprintf ( argBuffer, ">%04X", ( imm & 0x1FFF ) + base );
    return opcode + 2;
}

const UINT8 *InterpretFormat5 ( const UINT8 *opcode )
{
    isWord = *opcode++ & 0x01;
    strcpy ( argBuffer, GetSourceDestination ( opcode ));
    return opcode;
}

const UINT8 *InterpretFormat6 ( const UINT8 *opcode )
{
    char *ptr  = argBuffer;
    UINT8 bits = *opcode;
    UINT8 mode = ( UINT8 ) ( *opcode++ << 3 );

    if ( bits & MASK_NUMBER_IS_IMMEDIATE ) {
        UINT16 count = ( UINT16 ) (( opcode[0] << 8 ) | opcode[1] );
        ptr += sprintf ( ptr, "%d,", count );
        opcode += 2;
    } else {
        ptr += sprintf ( ptr, "%s,", GetSourceDestination ( opcode ));
    }

    char dest[25];
    strcpy ( dest, GetArgument ( mode, opcode ));
    mode <<= 2;
    if ( bits & MASK_DEST_IS_VDP_REGISTER ) {
        sprintf ( ptr, "%s,#%s", GetArgument ( mode, opcode ), dest + 4 );
    } else {
        sprintf ( ptr, "%s,%s", GetArgument ( mode, opcode ), dest );
    }
    return opcode;
}

const UINT8 *InterpretFormat7 ( const UINT8 *opcode )
{
    // FMT Instruction
    return opcode + 1;
}

UINT16 DisassembleGPL ( UINT16 address, const UINT8 *opcode, char *buffer )
{
    argBuffer [0] = '\0';
    isWord = 0;

    buffer += sprintf ( buffer, "%04X ", address );
    base = ( UINT16 ) ( address & 0xE000 );

    UINT16 retVal;

    unsigned i;
    for ( i = 0; i < SIZE ( GPL ); i++ ) {
        if (( *opcode >= GPL[i].lowOpCode ) && ( *opcode <= GPL[i].highOpCode )) break;
    }

    if ( GPL[i].Format == -1 ) {
        sprintf ( buffer, "<< Unknown GPL Command: %02X >>", *opcode );
        retVal = 1;
    } else {
        retVal = ( UINT16 ) ( GPL[i].Function ( opcode ) - opcode );
        sprintf ( buffer, "%s%-*.*s %s", isWord ? "D" : "", 6-isWord, 6-isWord,
                                            GPL[i].mnemonic, argBuffer );
    }

    return retVal;
}
