//----------------------------------------------------------------------------
//
// File:        dumpcpu.cpp
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
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "fileio.hpp"
#include "tms9900.hpp"
#include "ti994a.hpp"
#include "compress.hpp"
#include "option.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

#define BANK_SIZE	0x1000
#define BANK_MASK	0xF000

#define UNKNOWN		0x0000

#define DATA_BYTE	0x0001
#define DATA_WORD	0x0002
#define DATA_STRING	0x0004
#define DATA_TEXT	0x0008
#define DATA		0x000F

#define DATA_SINGLE	0x0010

#define PRESENT		0x0020

#define CODE		0x0040
#define CODE_START	0x0080

#define LABEL_REF	0x0200
#define LABEL_DEF	0x0400
#define LABEL_DATA	0x0800
#define LABEL_JMP	0x1000
#define LABEL_B		0x2000
#define LABEL_BL	0x4000
#define LABEL_BLWP	0x8000
#define LABEL		0xFE00

struct sREFDEF
{
    char    Name [7];
    UINT16  Address;    // Used while loading tagged-image files
};

char    LastLabel  [5] = "A@";

sREFDEF RefTable   [ 0x10000 ];
int     RefCount;

sREFDEF DefTable   [ 0x10000 ];
int     DefCount;

char   *Labels     [ 0x10000 ];
UINT16  Attributes [ 0x10000 ];
UINT8   Arguments  [ 0x10000 ];

extern "C" sLookUp LookUp [ 16 ];
extern sOpCode OpCodes [ 69 ];
extern "C" UINT8 CpuMemory [ 0x10000 ];

UINT8 *Memory = CpuMemory;

extern UINT16 DisassembleASM ( UINT16, const UINT8 *, char * );

FILE *outFile;

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

int sortFunction ( const sOpCode *p1, const sOpCode *p2 )
{
    if ( p1->opCode < p2->opCode ) return -1;
    if ( p1->opCode > p2->opCode ) return 1;
    return 0;
}

void Init ()
{
    FUNCTION_ENTRY ( NULL, "Init", true );

    memset ( Attributes, 0, sizeof ( Attributes ));

    // Sort the OpCode table by OpCode
    qsort ( OpCodes, SIZE ( OpCodes ), sizeof ( OpCodes[0] ), ( QSORT_FUNC ) sortFunction );

    // Create the LookUp table using the high 4 bits of each OpCode
    unsigned x, last = 0;
    for ( x = 0; x < SIZE ( LookUp ) - 1; x++ ) {
        int index = last + 1;
        while ( x + 1 > ( unsigned ) ( OpCodes[index].opCode >> 12 )) index++;
        LookUp [x].opCode = &OpCodes [last];
        LookUp [x].size = index - last - 1;
        last = index;
    }
    LookUp [x].opCode = &OpCodes [last];
    LookUp [x].size = SIZE ( OpCodes ) - last - 1;
}

struct sStackEntry {
    UINT16  address;
};

sStackEntry AddressStack [ 0x1000 ];
unsigned stackTop = 0;

bool PushAddress ( UINT16 address, UINT16 type, bool bottom = false )
{
    FUNCTION_ENTRY ( NULL, "PushAddress", true );

    if ( stackTop >= SIZE ( AddressStack )) {
        fprintf ( stderr, "========= ** ERROR ** Address Stack Overflow ** ERROR ** =========\n" );
        exit ( -1 );
    }

    if ( verbose ) fprintf ( stdout, "Pushing %04X - type: %04X bottom: %d  Current attributes: %04X\n", address, type, bottom, Attributes [ address ] );

    if ( bottom == false ) Attributes [ address ] |= type;

    if ( Attributes [ address ] & CODE ) return true;
    if (( Attributes [ address ] & DATA ) && ( type != LABEL_BLWP )) {
        DBG_WARNING ( "Pushed address " << hex << address << " marked as DATA" );
        return false;
    }

    DBG_TRACE ( "--- pushing address >" << hex << address );

    sStackEntry *pEntry = AddressStack;

    if ( bottom == true ) {
        memmove ( AddressStack + 1, AddressStack, sizeof ( sStackEntry ) * stackTop++ );
    } else {
        pEntry += stackTop++;
    }

    pEntry->address = address;

    return true;
}

bool IsStackEmpty ()
{
    FUNCTION_ENTRY ( NULL, "IsStackEmpty", true );

    return ( stackTop == 0 ) ? true : false;
}

bool PopAddress ( sStackEntry *entry )
{
    FUNCTION_ENTRY ( NULL, "PopAddress", true );

    if ( stackTop == 0 ) return false;

    *entry = AddressStack [ --stackTop ];

    if ( verbose ) fprintf ( stdout, "Popping %04X\n", entry->address );

    DBG_TRACE ( "--- popping address >" << hex << entry->address );

    return true;
}

//----------------------------------------------------------------------------
//
//  Standard Memory Header:
//    >x000 - >AA    Valid Memory Header Identification Code
//    >x001 - >xx    Version Number
//    >x002 - >xxxx  Number of Programs
//    >x004 - >xxxx  Address of Power Up Header
//    >x006 - >xxxx  Address of Application Program Header
//    >x008 - >xxxx  Address of DSR Routine Header
//    >x00A - >xxxx  Address of Subprogram Header
//    >x00C - >xxxx  Address of Interrupt Link
//    >x00E - >xxxx  Reserved
//
//  Header
//    >0000 - >xxxx Pointer to next Header
//    >0002 - >xxxx Start address for this object
//    >0004 - >xx   Name length for this object
//    >0005 - >     ASCII Name of this object
//
//----------------------------------------------------------------------------

struct sHeaderInfo {
    int         offset;
    const char *nameHdr;
    const char *nameLink;
};

bool InterpretHeader ( UINT16 start )
{
    FUNCTION_ENTRY ( NULL, "InterpretHeader", true );

   sHeaderInfo HeaderInfo[] = {
        { 0x0004, "PWRHDR%02X", "PWR%02X" },
        { 0x0006, "APPHDR%02X", "APP%02X" },
        { 0x0008, "DSRHDR%02X", "DSR%02X" },
        { 0x000A, "SUBHDR%02X", "SUB%02X" },
        { 0x000C, "INTHDR%02X", "INT%02X" }
    };

    if ( start == 0x0000 ) {
        if ( verbose ) fprintf ( stdout, "Pushing standard interrupt vectors\n" );
        PushAddress ( 0x0000, LABEL_BLWP, false );
        PushAddress ( 0x0004, LABEL_BLWP, false );
        PushAddress ( 0x0008, LABEL_BLWP, false );
        return true;
    }

    if ( Memory [ start ] != 0xAA ) return false;

    UINT16 *attr = Attributes + start;
    attr [ 0x0000 ] |= DATA_BYTE | DATA_SINGLE;
    attr [ 0x0001 ] |= DATA_BYTE | DATA_SINGLE;
    attr [ 0x0002 ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x0003 ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x0004 ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x0005 ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x0006 ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x0007 ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x0008 ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x0009 ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x000A ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x000B ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x000C ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x000D ] |= DATA_WORD | DATA_SINGLE;
    attr [ 0x000E ] |= DATA_WORD | DATA_SINGLE;    attr [ 0x000F ] |= DATA_WORD | DATA_SINGLE;

    // Interpret any headers we can find
    for ( size_t i = 0; i < SIZE ( HeaderInfo ); i++ ) {
        int offset = start + HeaderInfo [ i ].offset;
        UINT16 next = GetUINT16 ( Memory + offset );
        int j = 1;
        while ( next ) {
            char label [16];
            sprintf ( label, HeaderInfo [ i ].nameHdr, j );
            Labels [ next ] = strdup ( label );
            attr = Attributes + next;
            *attr++ |= DATA_WORD | DATA_SINGLE | LABEL;
            *attr++ |= DATA_WORD | DATA_SINGLE;
            UINT16 address;
            if ( isalpha ( Memory [ next + 4 ])) {	// Extended BASIC
                int len = Memory [ next + 2 ] + 1;
                while ( len-- ) *attr++ |= DATA_STRING;
                address = GetUINT16 ( Memory + next + 2 + len );
                *attr++ |= DATA_WORD | DATA_SINGLE;
                *attr   |= DATA_WORD | DATA_SINGLE;
            } else {					// TI BASIC
                address = GetUINT16 ( Memory + next + 2 );
                *attr++ |= DATA_WORD | DATA_SINGLE;
                *attr   |= DATA_WORD | DATA_SINGLE;
                int len = Memory [ next + 4 ] + 1;
                while ( len-- ) *attr++ |= DATA_STRING;
            }
            sprintf ( label, HeaderInfo [ i ].nameLink, j++ );
            Labels [ address ] = strdup ( label );
            PushAddress ( address, LABEL_B );
            next = GetUINT16 ( Memory + next );
        }
    }

    // Interrupt Link
    UINT16 address = GetUINT16 ( Memory + start + 0x000C );
    if ( address ) PushAddress ( address, LABEL_B );

    return true;
}

bool TraceCode ()
{
    FUNCTION_ENTRY ( NULL, "TraceCode", true );

    char buffer [ 80 ];

    sStackEntry top;

    bool codeTraced = false;

    if ( verbose ) fprintf ( stdout, "-- Tracing code --\n" );

    while ( PopAddress ( &top ) == true ) {

        UINT16 address = top.address;

        if ( Attributes [ address ] & LABEL_BLWP ) {
            if ( verbose ) fprintf ( stdout, "Handling BLWP label\n" );
            Attributes [ address ] |= DATA_WORD;
            Attributes [ address + 2 ] |= DATA_WORD;
            address = GetUINT16 ( Memory + address + 2 );
            Attributes [ address ] |= LABEL;
        }

        for ( EVER ) {

            if (( Attributes [ address ] & PRESENT ) == 0 ) {
                DBG_WARNING ( "No code loaded @ >" << hex << address );
                break;
            }

            codeTraced = true;

            // If we've already been here, skip it
            if ( Attributes [ address ] & CODE ) {
                if ( verbose ) fprintf ( stdout, ".. skipping %04X - already processed as CODE\n", address );
                if (( Attributes [ address ] & CODE_START ) == 0 ) {
                    fprintf ( stderr, "Error - code doesn't begin with CODE_START!\n" );
                }
                break;
            }

            UINT16 opcode = GetUINT16 ( Memory + address );

            UINT16 nextAddress = DisassembleASM ( address, &Memory [ address ], buffer );

            DBG_TRACE ( "    " << buffer );

            int size    = nextAddress - address;
            int badCode = 0;

            // Mark all bytes in this instruction as CODE

            Attributes [ address ] |= CODE_START;

            for ( int i = 0; ( badCode == 0 ) && ( i < size ); i++ ) {
                if ( Attributes [ address + i ] & CODE ) {
                    if ( verbose ) fprintf ( stdout, "Error parsing code - CODE attribute already set (%04X+%d)!\n", address, i );
                    badCode = i + 1;
                    break;
                }
                if (( i == 0 ) && ( Attributes [ address + i ] & DATA )) {
                    if ( verbose ) fprintf ( stdout, "Error parsing code - DATA attribute set (%04X+%d)!\n", address, i );
                    badCode = i + 1;
                    break;
                }
                if (( i != 0 ) && ( Attributes [ address + i ] & LABEL )) {
                    if ( verbose ) fprintf ( stdout, "Error parsing code - LABEL attribute set (%04X+%d)!\n", address, i );
                    badCode = i + 1;
                    break;
                }
                Attributes [ address + i ] |= CODE;
            }
            if ( badCode ) {
                Attributes [ address ] &= ~CODE_START;
                for ( int i = 0; i < badCode; i++ ) {
                    Attributes [ address + i ] &= ~CODE;
                }
                break;
            }

            // Look for B instructions
            if (( opcode & 0xFFC0 ) == 0x0440 ) {
                if ( opcode == 0x0460 ) {
                    UINT16 target = GetUINT16 ( Memory + address + 2 );
                    PushAddress ( target, LABEL_B );
                }
                if ( verbose ) fprintf ( stdout, ".. stopping trace at %04X - B\n", address );
                break;
            }

            // Look for BL instructions
            if (( opcode & 0xFFC0 ) == 0x0680 ) {
                if ( opcode == 0x06A0 ) {
                    UINT16 target = GetUINT16 ( Memory + address + 2 );
                    PushAddress ( target, LABEL_BL );
                    for ( int i = 0; i < Arguments [ target ]; i++ ) {
                        Attributes [ nextAddress + 2 * i ] |= DATA_WORD;
                        Attributes [ nextAddress + 2 * i + 1 ] |= DATA_WORD;
                    }
                    nextAddress += Arguments [ target ] * 2;
                }
            }

            // Look for BLWP instructions
            if (( opcode & 0xFFC0 ) == 0x0400 ) {
                UINT16 target = GetUINT16 ( Memory + address + 2 );
                PushAddress ( target, LABEL_BLWP );
                for ( int i = 0; i < Arguments [ target ]; i++ ) {
                    Attributes [ nextAddress + 2 * i ] |= DATA_WORD;
                    Attributes [ nextAddress + 2 * i + 1 ] |= DATA_WORD;
                }
                nextAddress += Arguments [ target ] * 2;
            }

            // Look for jump instructions (except NOP)
            if (( opcode > 0x1000 ) && ( opcode < 0x1D00 )) {
                UINT16 target = address + * ( char * ) &Memory [ address + 1 ] * 2 + 2;
                PushAddress ( target, LABEL_JMP );
                if (( opcode & 0xFF00 ) == 0x1000 ) {
                    if ( verbose ) fprintf ( stdout, ".. stopping trace at %04X - JMP\n", address );
                    break;
                }
            }

            // Look for RTWP instructions
            if ( opcode == 0x0380 ) {
                if ( verbose ) fprintf ( stdout, ".. stopping trace at %04X - RTWP\n", address );
                break;
            }

            address = nextAddress;
        }
    }

    return codeTraced;
}

UINT16 DumpByte ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpByte", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    char *tempPtr = buffer + sprintf ( buffer, "BYTE " );

    for ( int i = 0; i < 8; i++ ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s DATA marked as %s\n", address, "Byte", "CODE" );
        }
        tempPtr += sprintf ( tempPtr, ">%02X", Memory [ address ] );
        if ( ++address > limit ) break;
        if ( Attributes [ address - 1 ] & DATA_SINGLE ) break;
        if ( Attributes [ address ] != PRESENT ) {
            if (( Attributes [ address ] & DATA_BYTE ) == 0 ) break;
            if (( Attributes [ address ] & PRESENT ) == 0 ) break;
            if (( Attributes [ address ] & LABEL ) != 0 ) break;
        }
        if ( i < 7 ) *tempPtr++ = ',';
    }

    *tempPtr = '\0';

    return address;
}

UINT16 DumpWord ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpWord", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    char *tempPtr = buffer + sprintf ( buffer, "DATA " );

    for ( int i = 0; i < 8; i += 2 ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s DATA marked as %s\n", address, "Word", "CODE" );
        }
        tempPtr += sprintf ( tempPtr, ">%04X", GetUINT16 ( Memory + address ));
        address += 2;
        if ( Attributes [ address - 2 ] & DATA_SINGLE ) break;
        if ( address > limit ) break;
        if (( Attributes [ address ] & DATA_WORD ) == 0 ) break;
        if (( Attributes [ address ] & PRESENT ) == 0 ) break;
        if (( Attributes [ address ] & LABEL ) != 0 ) break;
        if ( i < 6 ) *tempPtr++ = ',';
    }

    return address;
}

UINT16 DumpString ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpString", true );

    int length = Memory [ address++ ];
    char *tempPtr = buffer + sprintf ( buffer, "STRI '" );

    for ( int i = 0; i < length; i++ ) {
        *tempPtr++ = Memory [ address++ ];
    }

    *tempPtr++ = '\'';
    *tempPtr = '\0';

    return address;
}

UINT16 DumpText ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpText", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    char *tempPtr = buffer + sprintf ( buffer, "BYTE '" );

    for ( int i = 0; i < 40; i++ ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s bytes marked as %s\n", address, "Text", "CODE" );
        }
        *tempPtr++ = Memory [ address++ ];
        if ( address > limit ) break;
        if (( Attributes [ address ] & PRESENT ) == 0 ) break;
        if (( Attributes [ address ] & DATA_TEXT ) == 0 ) break;
        if (( Attributes [ address ] & LABEL ) != 0 ) break;
    }

    *tempPtr++ = '\'';
    *tempPtr = '\0';

    return address;
}

void AddLabels ( char *dstBuffer, char *srcBuffer )
{
    FUNCTION_ENTRY ( NULL, "AddLabels", true );

    do {
        char *ptr = strchr ( srcBuffer, '>' );
        if (( ptr == NULL ) || ( ! isxdigit ( ptr [2] ))) {
            strcpy ( dstBuffer, srcBuffer );
            return;
        }

        strncpy ( dstBuffer, srcBuffer, ptr - srcBuffer );
        dstBuffer += ptr - srcBuffer;

        int size = isxdigit ( ptr [3] ) ? 4 : 2;
        unsigned int address;
        sscanf ( ptr, ( size == 4 ) ? ">%04X" : ">%02X", &address );

        if ( Labels [ address ] ) {
            strcpy ( dstBuffer, Labels [ address ] );
            dstBuffer += strlen ( dstBuffer );
            ptr += size;
        } else if ( Attributes [ address ] & LABEL ) {
            dstBuffer += sprintf ( dstBuffer, "L%0*X", size, address );
            ptr += size;
        } else {
            *dstBuffer++ = '>';
        }
        srcBuffer = ptr + 1;
    } while ( *srcBuffer );

    *dstBuffer = '\0';
}

UINT16 DumpCode ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpCode", true );

    char tempBuffer [ 256 ];

    bool isCode = (( Attributes [ address ] & CODE ) != 0 ) ? true : false;

    if (( isCode == true ) && (( Attributes [ address ] & CODE_START ) == 0 )) {
        fprintf ( stderr, "**ERROR->%04X** Instruction doesn't begin with CODE_START\n", address );
    }

    UINT16 nextAddress = DisassembleASM ( address, &Memory [ address ], tempBuffer );
    UINT16 size        = nextAddress - address;

    bool ok = true;
    for ( int i = 0; i < size; i++ ) {
        if (( isCode == true ) && (( Attributes [ address + i ] & CODE ) == 0 )) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction not marked as CODE\n", address, i );
            ok = false;
        }
        if ( i && ( Attributes [ address + i ] & CODE_START )) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction marked with CODE_START\n", address, i );
            ok = false;
        }
        if ( !i && Attributes [ address + i ] & DATA ) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction marked as DATA\n", address, i );
        }
    }

    ok = ( tempBuffer [1] == 'n' ) ? false : true;

    if ( ! ok ) {
        for ( int i = address; i < nextAddress; i++ ) {
            Attributes [ i ] = ( Attributes [ i ] & ~ ( CODE | CODE_START )) | DATA_WORD;
        }
        nextAddress = DumpWord ( address, buffer );
    } else {
        strcpy ( buffer, tempBuffer + 5 );
    }

    return nextAddress;
}

void DumpRam ( UINT32 start, UINT32 end, bool DefaultToCode )
{
    FUNCTION_ENTRY ( NULL, "DumpRam", true );

    fprintf ( outFile, "*\n" );
    fprintf ( outFile, "* Dump of ROM from %04X - %04X\n", start, end - 1 );
    fprintf ( outFile, "*\n" );

    UINT32 address     = start;
    UINT32 lastAddress = start;

    if ( RefCount > 0 ) fprintf ( outFile, "\n" );
    for ( int i = 0; i < RefCount; i++ ) {
        fprintf ( outFile, "\t\tREF  %s\n", RefTable [i].Name );
    }

    if ( DefCount > 0 ) fprintf ( outFile, "\n" );
    for ( int i = 0; i < DefCount; i++ ) {
        fprintf ( outFile, "\t\tDEF  %s\n", DefTable [i].Name );
    }
    if (( RefCount > 0 ) || ( DefCount > 0 )) fprintf ( outFile, "\n" );

    bool aorg = true;

    bool wasGood = true;
    int lastGood = address;

    while ( address < end ) {

        if (( Attributes [ address ] & PRESENT ) == 0 ) {
            if ( aorg == false ) fprintf ( outFile, "\n" );
            while ( address < end ) {
                address += 2;
                if (( Attributes [ address ] & PRESENT ) != 0 ) break;
            }
            if ( address >= end ) break;
            aorg = true;
        }

        if (( address & 0xFFF0 ) != lastAddress ) {
            lastAddress = address & 0xFFF0;
        }

        if ( aorg == true ) {
            fprintf ( outFile, "\t\tAORG >%04X\n\n", address );
            aorg = false;
        }

        char labelBuffer [ 32 ];
        if ( Labels [ address ] ) {
            strcpy ( labelBuffer, Labels [ address ] );
        } else if ( Attributes [ address ] & LABEL ) {
            sprintf ( labelBuffer, "L%04X", address );
        } else {
            strcpy ( labelBuffer, "" );
        }

        bool isGood = true;

        UINT16 nextAddress = 0;
        char rawCodeBuffer [ 256 ];
        if ( Attributes [ address ] & DATA ) {
            if ( Attributes [ address ] & DATA_STRING ) {
                nextAddress = DumpString (( UINT16 ) address, rawCodeBuffer );
            } else if ( Attributes [ address ] & DATA_TEXT ) {
                nextAddress = DumpText (( UINT16 ) address, rawCodeBuffer );
            } else if ( Attributes [ address ] & DATA_BYTE ) {
                nextAddress = DumpByte (( UINT16 ) address, rawCodeBuffer );
            } else if ( Attributes [ address ] & DATA_WORD ) {
                nextAddress = DumpWord (( UINT16 ) address, rawCodeBuffer );
            }
        } else if ( Attributes [ address ] & CODE ) {
            nextAddress = DumpCode (( UINT16 ) address, rawCodeBuffer );
        } else {
            isGood = false;
            if ( DefaultToCode == true ) {
                nextAddress = DumpCode (( UINT16 ) address, rawCodeBuffer );
            } else {
                nextAddress = DumpByte (( UINT16 ) address, rawCodeBuffer );
            }
        }

        if ( isGood != wasGood ) {
            if ( wasGood ) lastGood = address;
            if ( isGood && verbose ) fprintf ( stdout, "  Unparsed data @ %04X length: %d\n", lastGood, address - lastGood );
            fprintf ( outFile, "\n" );
            wasGood = isGood;
        }

        char codeBuffer [ 256 ];
        AddLabels ( codeBuffer, rawCodeBuffer );

        char dataBuffer [ 128 ];
        char *ptr = dataBuffer;

        if ( Attributes [ address ] & ( CODE | DATA_WORD )) {
            UINT16 index = ( UINT16 ) address;
            while ( index < nextAddress ) {
                ptr += sprintf ( ptr, ">%04X,", GetUINT16 ( Memory + index ));
                index += 2;
            }
            ptr [-1] = '\0';
        } else {
            UINT16 index = ( UINT16 ) address;
            ptr += sprintf ( ptr, "'" );
            while ( index < nextAddress ) {
                ptr += sprintf ( ptr, "%c", isprint ( Memory [ index ]) ? Memory [ index ] : '.' );
                index += 1;
            }
            ptr += sprintf ( ptr, "'" );
        }

        int tabs;
        tabs = 2 - strlen ( labelBuffer ) / 8;
        DBG_ASSERT ( tabs <= 5 );
        fprintf ( outFile, "%s%*.*s", labelBuffer, tabs, tabs, "\t\t\t\t\t" );
        tabs = 5 - strlen ( codeBuffer ) / 8;
        DBG_ASSERT ( tabs <= 5 );
        fprintf ( outFile, "%s%*.*s", codeBuffer, tabs, tabs, "\t\t\t\t\t" );
        tabs = 5 - ( strlen ( dataBuffer ) + 2 ) / 8;
        DBG_ASSERT ( tabs <= 5 );
        fprintf ( outFile, "* %s%*.*s", dataBuffer, tabs, tabs, "\t\t\t\t\t" );
        fprintf ( outFile, "* %04X\n", address );

        if (( UINT32 ) nextAddress < address ) break;

        address = nextAddress;
    }

    if ( ! wasGood && verbose ) fprintf ( stdout, "  Unparsed data @ %04X length: %d\n", lastGood, address - lastGood );
}

void ExtendRef ( UINT16 ref, UINT16 addr )
{
    // Find the end of the current chain
    while ( ref != 0x0000 ) {
        ref = GetUINT16 ( Memory + ref );
    }

    // Insert the new REF entry to the chain
    Memory [ref]   = ( addr >> 8 );
    Memory [ref+1] = addr;
}

UINT16 AllocateRef ( const char *name, UINT16 addr )
{
    FUNCTION_ENTRY ( NULL, "AllocateRef", true );

    // Look for an existing entry
    for ( int i = 0; i < RefCount; i++ ) {
        if ( strncmp ( RefTable [i].Name, name, 6 ) == 0 ) {
            // Update the REF table so we can call ResolveRef
            if ( RefTable [i].Address != 0 ) {
                // This REF hasn't been resolved yet
                ExtendRef ( RefTable [i].Address, addr );
            } else {
                RefTable [i].Address = addr;
            }
            return i;
        }
    }

    strncpy ( RefTable [RefCount].Name, name, 6 );
    RefTable [RefCount].Address = addr;

    return RefCount++;
}

UINT16 AllocateDef ( const char *name, UINT16 addr )
{
    FUNCTION_ENTRY ( NULL, "AllocateDef", true );

    // Look for an existing entry
    for ( int i = 0; i < DefCount; i++ ) {
        if ( strncmp ( DefTable [i].Name, name, 6 ) == 0 ) {
            fprintf ( stderr, "ERROR: Symbol '%6.6s' already exists in the DEF table\n", name );
            return i;
        }
    }

    strncpy ( DefTable [DefCount].Name, name, 6 );
    DefTable [DefCount].Address = addr;

    Attributes [ addr ] |= LABEL_REF;
    Labels[addr] = strndup ( ( const char * ) name, 6 );
    PushAddress ( addr, LABEL_REF );

    return DefCount++;
}

void ResolveRef ( sREFDEF *pRef, UINT16 def )
{
    UINT16 ref = pRef->Address;

    while ( ref != 0x0000 ) {
        UINT16 next = GetUINT16 ( Memory + ref );
        Memory [ ref ]   = ( def >> 8 );
        Memory [ ref+1 ] = def;
        Attributes [ ref ] |= LABEL_REF;
        ref = next;
    }

    pRef->Address = 0;
}

void ResolveRef ( const char *name )
{
    sREFDEF *pRef = NULL;

    for ( int i = 0; i < RefCount; i++ ) {
        if ( strncmp ( RefTable [i].Name, name, 6 ) == 0 ) {
            pRef = &RefTable [i];
        }
    }

    sREFDEF *pDef = NULL;

    for ( int i = 0; i < DefCount; i++ ) {
        if ( strncmp ( DefTable [i].Name, name, 6 ) == 0 ) {
            pDef = &DefTable [i];
        }
    }

    if (( pRef == NULL ) || ( pDef == NULL )) return;

    ResolveRef ( pRef, pDef->Address );
}

void ResolveRefs ( UINT16 base )
{
    for ( int i = 0; i < RefCount; i++ ) {
        if ( RefTable [i].Address != 0 ) {
            // Add a fake DEF - this will add a LABEL for us
            AllocateDef ( RefTable [i].Name, base );
            ResolveRef ( &RefTable [i], base );
            base += 2;
        }
    }
}

UINT16 Hex2Value ( UINT8 *&ptr, int fieldSize )
{
    FUNCTION_ENTRY ( NULL, "Hex2Value", true );

    int digit;
    UINT16 val = 0;

    if ( fieldSize == 4 ) {
        digit = *ptr++ - '0';
        if ( digit > 9 ) digit -= 'A' - '0' - 10;
        val = ( val << 4 ) + digit;
        digit = *ptr++ - '0';
        if ( digit > 9 ) digit -= 'A' - '0' - 10;
        val = ( val << 4 ) + digit;
        digit = *ptr++ - '0';
        if ( digit > 9 ) digit -= 'A' - '0' - 10;
        val = ( val << 4 ) + digit;
        digit = *ptr++ - '0';
        if ( digit > 9 ) digit -= 'A' - '0' - 10;
        val = ( val << 4 ) + digit;
    } else {
        val = *ptr++;
        val = ( val << 8 ) + *ptr++;
    }

    return val;
}

struct SEG
{
    char    Name [9];
    UINT16  Size;
    UINT16  Address;
};

SEG Segments[0x1000];

UINT16 CreateSegment ( const char *name, size_t len, UINT16 size, size_t index, UINT16 address )
{
    SEG *pSeg = &Segments[index+1];

    if ( pSeg->Size != 0 ) {
        fprintf ( stderr, "ERROR: Segment '%s' already defined\n", name );
        return pSeg->Address + pSeg->Size;
    }

    strncpy ( pSeg->Name, name, len+1 );
    pSeg->Size = size;
    pSeg->Address = address;

    return address + size;
}

const char *LoadTagged ( const char *fileName, UINT32 *rangeLo, UINT32 *rangeHi, UINT32 *start, bool allowOverwrite )
{
    FUNCTION_ENTRY ( NULL, "LoadFile", true );

    cFile *file = cFile::Open ( fileName, "disks" );
    if ( file == NULL ) return NULL;

    sFileDescriptorRecord *fdr = file->GetFDR ();

    // Make sure we have a DISPLAY/FIXED file (should we only support DIS/FIX 80?)
    if (( fdr->FileStatus & ( VARIABLE_TYPE | INTERNAL_TYPE | PROGRAM_TYPE )) != 0 ) {
        fprintf ( stderr, "PROGRAM/INTERNAL/VARIABLE file types are not supported\n" );
        return NULL;
    }

    char *progName = strdup ( fileName );

    UINT32 low  = 0x00010000;
    UINT32 high = 0x00000000;
    UINT32 next = 0x00002000;
    UINT32 addr = ( UINT32 ) -1;

    UINT32 badLo = 0x00010000;
    UINT32 badHi = 0x00000000;

    UINT8 buffer [256];
    if ( file->ReadRecord ( buffer, sizeof ( buffer )) == -1 ) {
        free ( progName );
        return NULL;
    }

    // See if it's a compressed file or not
    int fieldSize = 4;
    if ( buffer [0] == '\x01' ) {
        fieldSize = 2;
    }

    bool overwrite = false;

    // Read a TI-Tagged or TI-SDSMAC format file
    for ( EVER ) {
        bool done = false;
        UINT8 *ptr = buffer;
        while ( ! done ) {
            char tag = *ptr++;
            UINT16 field = Hex2Value ( ptr, fieldSize );
            switch ( tag ) {
                case 'K' :	// External Macro Reference (also Program Identification)
                    break;

                case  1  :
                case '0' :	// Program Identification - length - ProgramID
                    free ( progName );
                    progName = strndup (( char* ) ptr, 8 );
                    next = CreateSegment ( progName, 8, field, ( size_t ) -1, next );
                    ptr += 8;
                    break;
                case 'M' :
                    {
                        const char *name = ( const char * ) ptr;
                        ptr += 6;
                        size_t index = Hex2Value ( ptr, fieldSize );
                        next = CreateSegment ( name, 6, field, index, next );
                    }
                    break;

                case '2' :	// Entry Point Definition - Relocatable
                    field += Segments[0].Address;
                case '1' :	// Entry Point Definition - Absolute
                    Attributes [ addr ] |= LABEL_B;
                    *start = field;
                    break;

                case 'Z' :	// External SREFerences - xSEG Relocatable Symbol
                    field += Segments[Hex2Value ( ptr, fieldSize )+1].Address;      if ( 0 ) {
                case 'V' :	// External SREFerences - Relocatable Symbol
                    field += Segments[0].Address;                                   }
                case 'Y' :	// External SREFerences - Absolute Symbol
                    AllocateRef (( char * ) ptr, field );
                    ResolveRef (( char * ) ptr );
                    ptr += 6;
                    break;

                case 'X' :	// External REFerences - xSEG Relocatable Symbol
                    field += Segments[Hex2Value ( ptr, fieldSize )+1].Address;      if ( 0 ) {
                case '3' :	// External REFerences - Relocatable Symbol
                    field += Segments[0].Address;                                   }
                case '4' :	// External REFerences - Absolute Symbol
                    AllocateRef (( char * ) ptr, field );
                    ResolveRef (( char * ) ptr );
                    ptr += 6;
                    break;

                case 'W' :	// External DEFinitions - xSEG Relocatable Symbol
                    field += Segments[Hex2Value ( ptr, fieldSize )+1].Address;      if ( 0 ) {
                case '5' :	// External DEFinitions - Relocatable Symbol
                    field += Segments[0].Address;                                   }
                case '6' :	// External DEFinitions - Absolute Symbol
                    AllocateDef (( char * ) ptr, field );
                    ResolveRef (( char * ) ptr );
                    ptr += 6;
                    break;

                case '7' :	// Checksum Indicator - Checksum
                    break;
                case '8' :	// Checksum Ignore - any value
                    break;

                case 'P' :	// Load Address - CSEG Relocatable
                    field += Segments[Hex2Value ( ptr, fieldSize )+1].Address;      if ( 0 ) {
                case 'S' :	// Load Address - DSEG Relocatable
                    field += Segments[1].Address;                                   if ( 0 ) {
                case 'A' :	// Load Address - Relocatable
                    field += Segments[0].Address;                                   } }
                case '9' :	// Load Address - Absolute
                    addr = field;
                    break;

                case 'N' :	// Data - CSEG Relocatable
                    field += Segments[Hex2Value ( ptr, fieldSize )+1].Address;      if ( 0 ) {
                case 'T' :	// Data - DSEG Relocatable
                    field += Segments[1].Address;                                   if ( 0 ) {
                case 'C' :	// Data - Relocatable
                    field += Segments[0].Address;                                   } }
                    Attributes [ field ] |= LABEL_DATA;
                    Attributes [ addr ] |= DATA_WORD;
                case 'B' :	// Data - Absolute
                    if (( Attributes [ addr ] & PRESENT ) && ( Memory [ addr ] != ( field >> 8 ))) {
                        if ( allowOverwrite == false ) goto abort;
                        DBG_WARNING ( "Address " << hex << ( UINT16 ) addr << " is being overwritten (" << Memory [ addr ] << " -> " << ( UINT8 ) ( field >> 8 ) << ")" );
                        if ( verbose > 0 ) {
                            fprintf ( stdout, "Address %04X is being overwritten (%02X->%02X)\n", addr, Memory [ addr ], field >> 8 );
                        }
                        overwrite = true;
                        if ( addr > badHi ) badHi = addr;
                        if ( addr < badLo ) badLo = addr;
                    }
                    Attributes [ addr ] |= PRESENT;
                    Memory [ addr++ ] = ( UINT8 ) ( field >> 8 );
                    field <<= 8;
                    ptr += fieldSize / 2;
                case '*' :	// Data - byte (not used by the Editor/Assembler)
                    ptr -= fieldSize / 2;
                    if (( Attributes [ addr ] & PRESENT ) && ( Memory [ addr ] != ( field >> 8 ))) {
                        if ( allowOverwrite == false ) goto abort;
                        DBG_WARNING ( "Address " << hex << ( UINT16 ) addr << " is being overwritten (" << Memory [ addr ] << " -> " << ( UINT8 ) ( field >> 8 ) << ")" );
                        if ( verbose > 0 ) {
                            fprintf ( stdout, "Address %04X is being overwritten (%02X->%02X)\n", addr, Memory [ addr ], field >> 8 );
                        }
                        overwrite = true;
                        if ( addr > badHi ) badHi = addr;
                        if ( addr < badLo ) badLo = addr;
                    }
                    Attributes [ addr ] |= PRESENT;
                    Memory [ addr++ ] = ( UINT8 ) ( field >> 8 );
                    break;

                case 'F' :	// End of Record
                    done = true;
                    break;
                case ':' :	// End of File
                    goto abort;
                default :
                    fprintf ( stderr, "Unrecognized tag '%c' in file\n", tag );
            }
            if ( addr != ( UINT32 ) -1 ) {
                if ( addr > high ) high = addr;
                if ( addr < low ) low   = addr;
            }
        }
        if ( file->ReadRecord ( buffer, sizeof ( buffer )) == -1 ) break;
    }

abort:

    file->Release ( NULL );

    // Artificially resolve all outstanding REFs
    ResolveRefs ( next );

    // Mark memory in all segments as PRESENT
    for ( size_t i = 0; Segments [i].Size != 0; i++ ) {
        addr = Segments [i].Address;
        for ( size_t j = 0; j < Segments [i].Size; j++ ) {
            Attributes [ addr++ ] |= PRESENT;
        }
    }

    *rangeLo = max ( (UINT32) low, *rangeLo );
    *rangeHi = min ( (UINT32) high + 2, *rangeHi );

    if ( overwrite == true ) {
        fprintf ( stdout, "------------------------------------------------------------\n" );
        fprintf ( stdout, "WARNING: Memory in the range %04X-%04X has been overwritten.\n", badLo, badHi );
        fprintf ( stdout, "     Disassembly within this range may not be accurate.\n" );
        fprintf ( stdout, "    Try using '--no-overwrite' to disassemble this region.\n" );
        fprintf ( stdout, "------------------------------------------------------------\n" );
    }

    return progName;
}

struct sRange {
    UINT32 lo;
    UINT32 hi;
};

bool ParseRange ( const char *arg, void *ptr )
{
    FUNCTION_ENTRY ( NULL, "ParseRange", true );

    arg += strlen ( "range" ) + 1;

    unsigned int lo, hi;
    if ( sscanf ( arg, "%X-%X", &lo, &hi ) == 2 ) {
        sRange *range = ( sRange * ) ptr;
        range->lo = lo;
        range->hi = hi;
        return true;
    }

    return false;
}

bool ParseLoad ( const char *arg, void *val )
{
    FUNCTION_ENTRY ( NULL, "ParseLoad", true );

    arg += strlen ( "load" ) + 1;

    unsigned int num;
    if ( sscanf ( arg, "%X", &num ) == 1 ) {
        * ( int * ) val = num;
        return true;
    }

    return false;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: dumpcpu [options] file\n" );
    fprintf ( stdout, "\n" );
}

// ----------------------------------------------------------------------------

enum FILE_TYPE_E {
    FILE_UNKNOWN,
    FILE_CARTRIDGE,
    FILE_PROGRAM,
    FILE_TAGGED,
    FILE_IMAGE,
    FILE_MAX
};

bool IsValidPROGRAM ( cFile *file )
{
    FUNCTION_ENTRY ( NULL, "IsValidPROGRAM", true );

    UINT8 buffer [DEFAULT_SECTOR_SIZE];
    file->ReadRecord ( buffer, DEFAULT_SECTOR_SIZE );

    int flag = GetUINT16 ( buffer );
    if (( flag != 0x0000 ) && ( flag != 0xFFFF )) return false;

    int fileSize = file->FileSize ();
    int loadSize = GetUINT16 ( buffer + 2 );
    if (( fileSize != loadSize ) && ( fileSize != loadSize + 6 )) return false;

    return true;
}

bool IsValidTagged ( cFile *file )
{
    FUNCTION_ENTRY ( NULL, "IsValidTagged", true );

    sFileDescriptorRecord *fdr = file->GetFDR ();

    return ( fdr->RecordLength == 80 ) ? true : false;
}

FILE_TYPE_E DetermineFileType ( const char *name, char *fileName )
{
    FUNCTION_ENTRY ( NULL, "DetermineFileType", true );

    // Look for the file in all the places we might expect them
    const char *validName = LocateFile ( name, "cartridges" );
    if ( validName == NULL ) {
        validName = LocateFile ( name, "roms" );
    }

    // If we found a file, we need to figure out what type it is
    if ( validName != NULL ) {
        // Check to see if it's a valid cartridge
        cCartridge ctg ( validName );
        if ( ctg.IsValid () == true ) {
            strcpy ( fileName, validName );
            return FILE_CARTRIDGE;
        }

        // Look for a valid saved memory image
        sImageFileState info;
        if ( cTI994A::OpenImageFile ( validName, &info ) == true ) {
            strcpy ( fileName, validName );
            return FILE_IMAGE;
        }
    }

    // Look for a TI-Tagged file and PROGRAM images (on disks if necessary)
    cFile *file = cFile::Open (( validName == NULL ) ? name : validName, "disks" );
    if ( file == NULL ) {
        fprintf ( stderr, "Unable to locate file \"%s\"\n", name );
        return FILE_UNKNOWN;
    }

    FILE_TYPE_E fileType = FILE_UNKNOWN;

    sFileDescriptorRecord *fdr = file->GetFDR ();

    if (( fdr->FileStatus & ( VARIABLE_TYPE | INTERNAL_TYPE | PROGRAM_TYPE )) == PROGRAM_TYPE ) {
        if ( IsValidPROGRAM ( file )) {
            file->GetPath ( fileName, MAXPATH );
            fileType = FILE_PROGRAM;
        } else {
            fprintf ( stderr, "Invalid PROGRAM file \"%s\"\n", name );
        }
    }
    if (( fdr->FileStatus & ( VARIABLE_TYPE | INTERNAL_TYPE | PROGRAM_TYPE )) == 0 ) {
        if ( IsValidTagged ( file )) {
            file->GetPath ( fileName, MAXPATH );
            fileType = FILE_TAGGED;
        }
    }
    file->Release ( NULL );

    if ( fileType == FILE_UNKNOWN ) {
        fprintf ( stderr, "Unable to determine the type of \"%s\"\n", name );
    }

    return fileType;
}

// ----------------------------------------------------------------------------

const char *LoadCartridge ( const char *fileName, UINT32 *rangeLo, UINT32 *rangeHi )
{
    FUNCTION_ENTRY ( NULL, "LoadCartridge", true );

    cCartridge ctg ( fileName );
    if ( ctg.IsValid () == false ) {
        fprintf ( stderr, "\"%s\" is not a valid cartridge file\n", fileName );
        return NULL;
    }

    ctg.PrintInfo ( stdout );

    int lo = -1;
    int hi = 0;

    for ( unsigned i = 0; i < SIZE ( ctg.CpuMemory ); i++ ) {
        if ( ctg.CpuMemory[i].NumBanks > 0 ) {
            if ( lo == -1 ) lo = i;
            hi = i + 1;
            UINT8 *ptr = ctg.CpuMemory[i].Bank[0].Data;
            memcpy (( char * ) &Memory [ i * BANK_SIZE ], ( char * ) ptr, BANK_SIZE );
            for ( unsigned j = 0; j < BANK_SIZE; j++ ) {
                Attributes [ i * BANK_SIZE + j ] |= PRESENT;
            }
        }
    }

    *rangeLo = max ( (UINT32) lo * BANK_SIZE, *rangeLo );
    *rangeHi = min ( (UINT32) hi * BANK_SIZE, *rangeHi );

    return strdup ( ctg.Title ());
}

// ----------------------------------------------------------------------------

static void GetNextFileName ( const char *fileName, int index, char *name )
{
    strcpy ( name, fileName );
    char *lastChar = name + strlen ( name ) - 1;
    *lastChar = *lastChar + 1;
}

const char *LoadPROGRAM ( const char *fileName, UINT32 *rangeLo, UINT32 *rangeHi, UINT32 *start )
{
    FUNCTION_ENTRY ( NULL, "LoadPROGRAM", true );

    char name [MAXPATH];
    strcpy ( name, fileName );

    cFile *file = cFile::Open ( name, "disks" );

    char *retName = strdup ( "PROGRAM" );

    for ( int i = 0; file != NULL; i++ ) {

        UINT8 buffer [DEFAULT_SECTOR_SIZE];
        int count = file->ReadRecord ( buffer, DEFAULT_SECTOR_SIZE );

        int more    = GetUINT16 ( buffer );
        int address = GetUINT16 ( buffer + 4 );

        if ( i == 0 ) *start = address;

        count -= 6;
        memcpy (( char * ) &Memory [ address ], ( char * ) buffer + 6, count );
        for ( int j = 0; j < count; j++ ) {
            Attributes [ address + j ] |= PRESENT;
        }

        do {
            address += count;
            count = file->ReadRecord ( Memory + address, DEFAULT_SECTOR_SIZE );
            for ( int j = 0; j < count; j++ ) {
                Attributes [ address + j ] |= PRESENT;
            }
        } while ( count > 0 );

        GetNextFileName ( fileName, i, name );

        file->Release ( NULL );

        if ( more == 0 ) break;

        file = cFile::Open ( name, "disks" );
    }

    return retName;
}

// ----------------------------------------------------------------------------

const char *LoadImageFile ( const char *fileName, UINT32 *rangeLo, UINT32 *rangeHi, bool scratchpad )
{
    FUNCTION_ENTRY ( NULL, "LoadImageFile", true );

    sImageFileState info;
    if ( cTI994A::OpenImageFile ( fileName, &info ) == false ) {
        fprintf ( stderr, "\"%s\" is not a valid save image file\n", fileName );
        return NULL;
    }

    if ( cTI994A::FindHeader ( &info, SECTION_ROM ) == false ) {
        fprintf ( stderr, "File \"%s\" does not contain a ROM section\n", fileName );
        return NULL;
    }

    UINT8 dummy [ ROM_BANK_SIZE * 2 ];

    for ( unsigned i = 0; i < 16; i++ ) {
        UINT8 numBanks = fgetc ( info.file );
        if ( numBanks == 0 ) continue;
        fgetc ( info.file );         // Skip over the current bank index
        if ( fgetc ( info.file ) == 1 ) {
            LoadBuffer ( BANK_SIZE, &Memory [ i * BANK_SIZE ], info.file );
            if ( i != 9 ) {
                unsigned start = ( i == 8 ) ? 0x0300 : 0;
                unsigned size  = ( i == 8 ) ? 0x0400 : BANK_SIZE;
                for ( unsigned j = start; j < size; j++ ) {
                    Attributes [ i * BANK_SIZE + j ] |= PRESENT;
                }
            }
        }
        for ( int j = 0; j < numBanks; j++ ) {
            if ( fgetc ( info.file ) == 0 ) continue;
            LoadBuffer ( BANK_SIZE, dummy, info.file );
        }
    }

    fclose ( info.file );

    // Clear out empty sections
    int start = 0;
    for ( int i = 0; i < 0x10000; i++ ) {
        if ( Memory [i] == 0 ) continue;
        if ( i - start > 64 ) {
            for ( int x = start + 1; x < i; x++ ) {
                Attributes [x] &= ~PRESENT;
            }
        }
        start = i;
    }

    if ( scratchpad == false ) {
        for ( int i = 0x8300; i < 0x8400; i++ ) {
            Attributes [i] &= ~PRESENT;
        }
    }

    UINT32 lo = 0x10000;
    UINT32 hi = 0x00000;

    for ( int i = 0; i < 0x10000; i++ ) {
        if ((( Attributes [ i ] & PRESENT ) != 0 ) && ( Memory [i] != 0 )) {
            lo = i;
            break;
        }
    }

    for ( int i = 0xFFFF; i >= 0; i-- ) {
        if ((( Attributes [ i ] & PRESENT ) != 0 ) && ( Memory [i] != 0 )) {
            hi = i;
            break;
        }
    }

    *rangeLo = max ( lo, *rangeLo );
    *rangeHi = min ( hi, *rangeHi );

    return strdup ( "<Save Image File>" );
}

// ----------------------------------------------------------------------------

bool g_LinePresent;
char g_LineBuffer [256];

static void GetLine ( char *buffer, int size, FILE *file )
{
    FUNCTION_ENTRY ( NULL, "GetLine", true );

    do {
        if ( g_LinePresent ) {
            g_LinePresent = false;
            strcpy ( buffer, g_LineBuffer );
        } else {
            if ( fgets ( buffer, size, file ) == NULL ) return;
        }
    } while (( buffer [0] == '*' ) || ( buffer [0] == '\0' ));
}

static void UnGetLine ( char *buffer )
{
    FUNCTION_ENTRY ( NULL, "UnGetLine", true );

    strcpy ( g_LineBuffer, buffer );
    g_LinePresent = true;
}

void ParseEQU ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseEQU", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        unsigned int address;
        if ( sscanf ( ptr, "%04X", &address ) != 1 ) {
            fprintf ( stderr, "Error parsing line '%s'", buffer );
            break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }

    }
}

void ParseCode ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseCode", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        unsigned int address;
        if ( sscanf ( ptr, "%04X", &address ) != 1 ) {
            fprintf ( stderr, "Error parsing line '%s'", buffer );
            break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }
        PushAddress ( address, 0, false );

    }
}

void ParseData ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseData", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address, type, and length
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        char type [256];
        unsigned int address, count;
        switch ( sscanf ( ptr, "%04X %s %04X", &address, type, &count )) {
            case 1 :
                strcpy ( type, "BYTE" );
            case 2 :
                count = 1;
                break;
            case 3 :
                break;
            default :
                fprintf ( stderr, "Error parsing line '%s'", buffer );
                break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }

        int attr = DATA_BYTE;
        if ( stricmp ( type, "BYTE" ) == 0 ) {
            attr = DATA_BYTE;
        }
        if ( stricmp ( type, "WORD" ) == 0 ) {
            attr = DATA_WORD;
            count *= 2;
        }
        if ( stricmp ( type, "STRING" ) == 0 ) {
            attr = DATA_STRING;
        }
        if ( stricmp ( type, "TEXT" ) == 0 ) {
            attr = DATA_TEXT;
        }

        Attributes [address] |= LABEL_DATA;
        for ( unsigned int i = 0; i < count; i++ ) {
            Attributes [address+i] |= attr;
        }
    }
}

void ParseSubroutine ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseSubroutine", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        unsigned int address;
        int count;
        switch ( sscanf ( ptr, "%04X %d", &address, &count )) {
            case 1 :
                count = 0;
                break;
            case 2 :
                break;
            default :
                fprintf ( stderr, "Error parsing line '%s'", buffer );
                break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }
        PushAddress ( address, LABEL_BL, false );
        Arguments  [ address ] = count;
    }
}

void ParseBLWP ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseBLWP", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        unsigned int address;
        int count;
        switch ( sscanf ( ptr, "%04X %d", &address, &count )) {
            case 1 :
                count = 0;
                break;
            case 2 :
                break;
            default :
                fprintf ( stderr, "Error parsing line '%s'", buffer );
                break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }
        PushAddress ( address, LABEL_BLWP, false );
        Arguments  [ address ] = count;
    }
}

void ParseTable ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseTable", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
        char *ptr = buffer;

        // Look for a label
        int symLength = 0;
        char symbol [256];
        while ( ! isspace ( *ptr )) {
            symbol [ symLength++ ] = *ptr++;
        }
        symbol [ symLength ] = 0;

        // Get the address, type, and length
        while ( isspace ( *ptr )) {
            ptr++;
        }

        if ( *ptr == '\0' ) {
            continue;
        }

        unsigned int address, count;
        switch ( sscanf ( ptr, "%04X %04X", &address, &count )) {
            case 1 :
                count = 1;
                break;
            case 2 :
                break;
            default :
                fprintf ( stderr, "Error parsing line '%s'", buffer );
                break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }

        Attributes [address] |= LABEL_DATA;
        for ( unsigned int i = 0; i < 2 * count; i += 2 ) {
            Attributes [address+i] |= DATA_WORD;
            Attributes [address+i+1] |= DATA_WORD;
            UINT16 code = GetUINT16 ( Memory + address + i );
            PushAddress ( code, LABEL_BL, false );
        }

    }
}

void ParseREF ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseREF", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
    }
}

void ParseDEF ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "ParseDEF", true );

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] == '[' ) {
            UnGetLine ( buffer );
            break;
        }
    }
}

void ReadConfig ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "ReadConfig", true );

    FILE *file = fopen ( fileName, "rt" );
    if ( file == NULL ) return;

    for ( EVER ) {

        char buffer [256];
        GetLine ( buffer, sizeof ( buffer ), file );
        if ( feof ( file )) break;

        if ( buffer [0] != '[' ) continue;
        if ( strnicmp ( buffer + 1, "equ", 3 ) == 0 ) {
            ParseEQU ( file );
        }
        if ( strnicmp ( buffer + 1, "code", 4 ) == 0 ) {
            ParseCode ( file );
        }
        if ( strnicmp ( buffer + 1, "data", 4 ) == 0 ) {
            ParseData ( file );
        }
        if ( strnicmp ( buffer + 1, "subroutine", 10 ) == 0 ) {
            ParseSubroutine ( file );
        }
        if ( strnicmp ( buffer + 1, "blwp", 4 ) == 0 ) {
            ParseBLWP ( file );
        }
        if ( strnicmp ( buffer + 1, "table", 5 ) == 0 ) {
            ParseTable ( file );
        }
        if ( strnicmp ( buffer + 1, "ref", 3 ) == 0 ) {
            ParseREF ( file );
        }
        if ( strnicmp ( buffer + 1, "def", 3 ) == 0 ) {
            ParseDEF ( file );
        }
    }

    fclose ( file );
}

// ----------------------------------------------------------------------------

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    bool allowOverwrite = true;
    bool defaultToCode  = false;
    bool loadImage      = false;
    bool scratchpad     = false;
    sRange range        = { 0x00000, 0x10000 };
    UINT32 loadStart    = ( UINT32 ) -1;

    sOption optList [] = {
        {  0,  "code",          OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &defaultToCode,  NULL,       "Treat data as CODE by default" },
        {  0,  "image",         OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &loadImage,      NULL,       "Treat 'file' as a memory image file" },
        {  0,  "no-overwrite",  OPT_VALUE_SET | OPT_SIZE_BOOL, false, &allowOverwrite, NULL,       "Stop loading DF80 files when data is overwriten" },
        {  0,  "range*=lo-hi",  OPT_NONE,                      0,     &range,          ParseRange, "Only dump the indicate range" },
        {  0,  "load*=address", OPT_NONE,                      0,     &loadStart,      ParseLoad,  "Treat address as the start of code" },
        {  0,  "scratch-pad",   OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &scratchpad,     NULL,       "Include scratch pad RAM in disassembly" },
        { 'v', "verbose",       OPT_VALUE_SET | OPT_SIZE_BOOL, true,  &verbose,        NULL,       "Display information about the file being analyzed" }
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "TI-99/4A TMS9900 Disassembler\n\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    Init ();

    const char *moduleName = NULL;

    char fileName [ 256 ];

    switch ( DetermineFileType ( argv [index], fileName )) {
        case FILE_CARTRIDGE :
            moduleName = LoadCartridge ( fileName, &range.lo, &range.hi );
            break;
        case FILE_PROGRAM :
            moduleName = LoadPROGRAM ( fileName, &range.lo, &range.hi, &loadStart );
            break;
        case FILE_TAGGED :
            moduleName = LoadTagged ( fileName, &range.lo, &range.hi, &loadStart, allowOverwrite );
            break;
        case FILE_IMAGE :
            moduleName = LoadImageFile ( fileName, &range.lo, &range.hi, scratchpad );
            break;
        default :
            return -1;
    }

    if ( moduleName == NULL ) {
        return -1;
    }

    if ( range.lo >= range.hi ) {
        printf ( "This file does not contain any data\n" );
        return 0;
    }

    const char *_start = strrchr ( fileName, FILE_SEPERATOR );
    _start = ( _start == NULL ) ? fileName : _start + 1;

    char baseName [ 256 ];
    strcpy ( baseName, _start );

    char *ptr = strrchr ( baseName, '.' );
    if ( ptr != NULL ) *ptr = '\0';

    sprintf ( fileName, "%s.%s", baseName, "cfg" );
    ReadConfig ( fileName );

    sprintf ( fileName, "%s.%s", baseName, "asm" );
    outFile = fopen ( fileName, "wt" );
    if ( outFile == NULL ) {
        fprintf ( stderr, "Unable to open file \"%s\" for writing\n", fileName );
        return -1;
    }

    bool headerFound = false;
    fprintf ( stdout, "Headers located:" );
    for ( unsigned i = 0; i < 16; i++ ) {
        if (( Attributes [ i * BANK_SIZE ] & PRESENT ) != 0 ) {
            if ( InterpretHeader ( i * BANK_SIZE ) == true ) {
                headerFound = true;
                fprintf ( stdout, " %04X", i * BANK_SIZE );
            }
        }
    }
    fprintf ( stdout, ( headerFound == false ) ? " -NONE-\n\n" : "\n\n" );

    if ( loadStart != ( UINT32 ) -1 ) {
        if ( Labels [ loadStart ] == NULL ) {
            Labels [ loadStart ] = strdup ( "START" );
        }
        PushAddress (( UINT16 ) loadStart, LABEL_JMP );
    }

    TraceCode ();

    fprintf ( outFile, "*\n" );
    fprintf ( outFile, "* Module: %s\n", moduleName );
    fprintf ( outFile, "*\n" );

    DumpRam ( range.lo, range.hi, defaultToCode );

    fclose ( outFile );

    free (( void * ) moduleName );

    return 0;
}
