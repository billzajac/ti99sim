//----------------------------------------------------------------------------
//
// File:        disassemble.cpp
// Date:        23-Feb-1998
// Programmer:  Marc Rousseau
//
// Description: Simple disassembler for the TMS9900 processor
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
#include "tms9900.hpp"

static int     bUseR = 1;
static char   *pBuffer;
static UINT16  ByteTable [ 256 + 1 ];

extern "C" sLookUp LookUp [ 16 ];

static bool InitByteTable ()
{
    for ( unsigned  i = 0; i < SIZE ( ByteTable ) - 1; i++ ) {
        sprintf (( char * ) &ByteTable [i], "%02X", i );
    }

    return true;
}

static bool initialized = InitByteTable ();

static void AddDigit ( int num )
{
    static const char *values[17] = {
        "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16"
    };

    memcpy( pBuffer, values[num], 2 );
    pBuffer += ( num < 10 ) ? 1 : 2;
}

static void AddReg ( int reg )
{
    static const char *registers[16] = {
        "R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"
    };

    if ( bUseR ) {
        memcpy( pBuffer, registers[reg], 3 );
        pBuffer += ( reg < 10 ) ? 2 : 3;
    } else {
        AddDigit( reg );
    }
}

static void AddByte ( UINT8 data )
{
    memcpy ( pBuffer, ByteTable + data, 2 );
    pBuffer += 2;
}

static void AddWord ( UINT16 data )
{
    memcpy ( pBuffer, ByteTable + ( data >> 8 ), 2 );
    memcpy ( pBuffer + 2, ByteTable + ( data & 0x00FF ), 2 );
    pBuffer += 4;
}

static void GetRegs ( UINT16 op, UINT16 data )
{
    int reg = op & 0x0F;
    int mode = ( op >> 4 ) & 0x03;
    switch ( mode ) {
        case 0 : AddReg ( reg );
                 break;
        case 2 : *pBuffer++ = '@';
                 *pBuffer++ = '>';
                 AddWord ( data );
                 if ( reg != 0 ) {
                     *pBuffer++ = '(';
                     AddReg ( reg );
                     *pBuffer++ = ')';
                 }
                 break;
        case 1 :
        case 3 : *pBuffer++ = '*';
                 AddReg ( reg );
                 if ( mode == 3 ) *pBuffer++ = '+';
                 break;
    }
}

static void format_I ( UINT16 opcode, UINT16 arg1, UINT16 arg2 )
{
    GetRegs ( opcode, arg1 );
    *pBuffer++ = ',';
    GetRegs (( UINT16 ) ( opcode >> 6 ), arg2 );
}

static void format_II ( UINT16 opcode, UINT16 PC )
{
    if ( opcode == 0x1000 ) {
        strcpy ( pBuffer - 5, "NOP" );
    } else {
        char disp = ( char ) opcode;
        *pBuffer++ = '>';
        if ( opcode >= 0x1D00 ) {
            AddByte (( UINT8 ) disp );
        } else {
            AddWord (( UINT16 ) ( PC + disp * 2 ));
        }
    }
}

static void format_III ( UINT16 opcode, UINT16 arg1 )
{
    GetRegs ( opcode, arg1 );
    *pBuffer++ = ',';
    AddReg (( opcode >> 6 ) & 0xF );
}

static void format_IV ( UINT16 opcode, UINT16 arg1 )
{
    GetRegs ( opcode, arg1 );
    UINT8 disp = ( UINT8 ) (( opcode >> 6 ) & 0xF );
    *pBuffer++ = ',';
    AddDigit ( disp ? disp : 16 );
}

static void format_V ( UINT16 opcode )
{
    AddReg ( opcode & 0xF );
    *pBuffer++ = ',';
    AddDigit (( opcode >> 4 ) & 0xF );
}

static void format_VI ( UINT16 opcode, UINT16 arg1 )
{
    GetRegs ( opcode, arg1 );
}

static void format_VIII ( UINT16 opcode, UINT16 arg1 )
{
    if ( opcode < 0x02A0 ) {
        AddReg ( opcode & 0x000F );
        *pBuffer++ = ',';
        *pBuffer++ = '>';
        AddWord ( arg1 );
    } else if ( opcode >= 0x02E0 ) {
        *pBuffer++ = '>';
        AddWord ( arg1 );
    } else {
        AddReg ( opcode & 0x000F );
    }
}

static inline UINT16 GetWord ( const UINT8 *ptr )
{
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

static int GetArgs ( UINT16 PC, const UINT16 *ptr, const sOpCode *op, UINT16 opcode )
{
    UINT16 arg1 = 0, arg2 = 0;
    void (*format) ( UINT16, UINT16 );
    int index = 0;
    switch ( op->format ) {
        case 1 :				// Two General Addresses
                 if (( opcode & 0x0030 ) == 0x0020 ) arg1 = GetWord (( UINT8 * ) &ptr [++index] );
                 if (( opcode & 0x0C00 ) == 0x0800 ) arg2 = GetWord (( UINT8 * ) &ptr [++index] );
                 format_I ( opcode, arg1, arg2 );
                 return index * 2;
        case 2 : format_II ( opcode, PC );
                 return 0;
        case 5 : format_V ( opcode );
                 return 0;
        case 3 : format = format_III;   break;	// Logical
        case 4 : format = format_IV;    break;	// CRU Multi-Bit
        case 6 : format = format_VI;    break;	// Single Address
        case 9 : format = format_III;   break;	// XOP, MULT, & DIV
        case 8 :				// Immediate
                 if (( opcode < 0x02A0 ) || ( opcode >= 0x02E0 )) arg1 = GetWord (( UINT8 * ) &ptr [++index] );
                 format_VIII ( opcode, arg1 );
                 return index * 2;
        case 7 : //format_VII ( opcode, arg1, arg2 );
                 return 0;
        default :
                 return 0;
    }

    if (( opcode & 0x0030 ) == 0x0020 ) arg1 = GetWord (( UINT8 * ) &ptr [++index] );
    format ( opcode, arg1 );

    return index * 2;
}

UINT16 DisassembleASM ( UINT16 PC, const UINT8 *ptr, char *buffer )
{
    pBuffer = buffer;
    AddWord ( PC );
    *pBuffer++ = ' ';

    if ( PC & 1 ) {
        strcpy ( pBuffer, "<-- Illegal value in PC" );
        return ( UINT16 ) ( PC + 1 );
    }

    UINT16 curOpCode = GetWord ( ptr );
    PC += ( UINT16 ) 2;

    sLookUp *lookup = &LookUp [ curOpCode >> 12 ];
    sOpCode *op = lookup->opCode;
    int retries = lookup->size;
    if ( retries ) {
        while (( curOpCode & op->mask ) != op->opCode ) {
            op++;
            if ( retries-- == 0 ) {
                strcpy ( pBuffer, "Invalid Op-Code" );
                return PC;
            }
        }
    }

    memcpy ( pBuffer, op->mnemonic, 4 ), pBuffer += 4;

    if ( op->format != 7 ) {
        *pBuffer++ = ' ';
        PC += ( UINT16 ) GetArgs ( PC, ( UINT16 * ) ptr, op, curOpCode );
    }

    *pBuffer = '\0';

    return PC;
}
