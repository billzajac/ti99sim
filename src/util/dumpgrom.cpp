//----------------------------------------------------------------------------
//
// File:        dumpgrom.cpp
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
#include "support.hpp"
#include "option.hpp"

DBG_REGISTER ( __FILE__ );

#define BANK_SIZE	0x2000
#define BANK_MASK	0xE000

#define UNKNOWN		0x0000

#define DATA_BYTE	0x0001
#define DATA_WORD	0x0002
#define DATA_STRING	0x0004
#define DATA_TEXT	0x0008
#define DATA		0x000F

#define DATA_SINGLE	0x0010

#define PRESENT		0x0020

#define CODE		0x0100
#define CODE_START	0x0200

#define FMT_DATA	0x0400

#define LABEL		0x8000

char   LastLabel  [5] = "A@";

char  *Labels     [ 0x10000 ];
UINT16 Attributes [ 0x10000 ];
UINT8  Memory     [ 0x10000 ];
char   DataCount  [ 0x10000 ];

extern UINT16 DisassembleGPL ( UINT16, const UINT8 *, char * );

FILE *outFile;

void Init ()
{
    FUNCTION_ENTRY ( NULL, "Init", true );

    memset ( Attributes, 0, sizeof ( Attributes ));
}

char *GetNextLabel ()
{
    FUNCTION_ENTRY ( NULL, "GetNextLabel", true );

    if ( LastLabel [1] == 'Z' ) {
        LastLabel [0]++;
        LastLabel [1] = 'A';
    } else {
        LastLabel [1]++;
    }
    return strdup ( LastLabel );
}

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

enum eReason {
    TYPE_BRANCH,
    TYPE_CALL,
    TYPE_POST_CALL
};

struct sStackEntry {
    UINT16  address;
    eReason reason;
    UINT16  source;     // The address of the preceeding subroutine call in the case of TYPE_POST_CALL
};

sStackEntry AddressStack [ 0x1000 ];
unsigned stackTop = 0;

bool PushAddress ( UINT16 address, eReason reason, UINT16 source )
{
    FUNCTION_ENTRY ( NULL, "PushAddress", true );

    if ( stackTop >= SIZE ( AddressStack )) {
        fprintf ( stderr, "========= ** ERROR ** Address Stack Overflow ** ERROR ** =========\n" );
        return false;
    }

    if ( reason != TYPE_POST_CALL ) Attributes [ address ] |= LABEL;

    if ( Attributes [ address ] & CODE ) return true;
    if ( Attributes [ address ] & DATA ) {
        DBG_WARNING ( "Pushed address " << hex << address << " marked as DATA" );
        return false;
    }

    DBG_TRACE ( "--- pushing address >" << hex << address );

    sStackEntry *pEntry = AddressStack + stackTop++;

    pEntry->address = address;
    pEntry->reason  = reason;
    pEntry->source  = source;

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

bool InterpretHeader ( UINT16 start )
{
    FUNCTION_ENTRY ( NULL, "InterpretHeader", true );

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
    for ( int offset = start + 0x0004; offset < start + 0x000C; offset += 2 ) {
        UINT16 next = GetUINT16 ( Memory + offset );
        while ( next ) {
            attr = Attributes + next;
            *attr++ = DATA_WORD | DATA_SINGLE | LABEL;
            *attr++ = DATA_WORD | DATA_SINGLE;
            UINT16 address;
            if ( isalpha ( Memory [ next + 4 ])) {  	// Extended BASIC
                int len = Memory [ next + 2 ] + 1;
                while ( len-- ) *attr++ = DATA_STRING;
                address = GetUINT16 ( Memory + next + 2 + len );
                *attr++ = DATA_WORD | DATA_SINGLE;
                *attr   = DATA_WORD | DATA_SINGLE;
            } else {					// TI BASIC
                address = GetUINT16 ( Memory + next + 2 );
                *attr++ = DATA_WORD | DATA_SINGLE;
                *attr   = DATA_WORD | DATA_SINGLE;
                int len = Memory [ next + 4 ] + 1;
                while ( len-- ) *attr++ = DATA_STRING;
            }
            PushAddress ( address, TYPE_BRANCH, ( UINT16 ) -1 );
            next = GetUINT16 ( Memory + next );
        }
    }

    // Interrupt Link
    UINT16 address = GetUINT16 ( Memory + 0x000C );
    if ( address ) PushAddress ( address, TYPE_BRANCH, ( UINT16 ) -1 );

    return true;
}

int FindFormatData ( UINT16 start )
{
    FUNCTION_ENTRY ( NULL, "FindFormatData", true );

    int level = 1;

    UINT8 *ptr   = &Memory [ start ];
    UINT16 *attr = &Attributes [ start ];

    while ( level ) {
        if ( *attr != PRESENT ) break;
        UINT8 code = *ptr;
        int size = 1;
        int count = ( code & 0x1F ) + 1;
        switch ( code >> 5 ) {
            case 0 : // string accross ( HTEX / HSTR )
            case 1 : // string down ( VTEX / VSTR )
                size += count;
                break;
            case 2 : // repeat accross ( HCHA )
            case 3 : // repeat down ( VCHA )
                size += 1;
                break;
            case 6 : // repeat block ( FOR )
                level++;
                break;
            case 7 : // special ( FEND / ROW / COL )
                switch ( code ) {
                    case 0xFB : // FEND
                        level--;
                        break;
                    case 0xFC : // BIAS
                    case 0xFD : // ??BIAS
                    case 0xFE : // YPT
                    case 0xFF : // XPT
                        size += 1;
                        break;
                    default :   // ???
//                        size += count;
                        size++;
                        break;
                }
                break;
        }
        ptr += size;
        attr += size;
    }


    if ( ptr [-1] != 0xFB ) {
        fprintf ( stderr, "Error - FMT section >%04X->%04X doesn't end in FEND\n", start - 1, ( int ) ( start + ptr - &Memory [ start ] - 1 ));
        Attributes [ start -1 ] &= ~( CODE | CODE_START );
    } else {
        int size = ptr - &Memory [ start ];
        attr = &Attributes [ start ];
        while ( size-- ) *attr++ |= FMT_DATA;
    }

    return ptr - &Memory [ start ];
}

void TraceCode ()
{
    FUNCTION_ENTRY ( NULL, "TraceCode", true );

    char buffer [ 80 ];

    sStackEntry top;
    while ( PopAddress ( &top ) == true ) {

        UINT16 address = top.address;

        if ( top.reason == TYPE_POST_CALL ) {
            int data = DataCount [ top.source ];
            while ( data-- ) {
                Attributes [ address++ ] |= DATA_BYTE;
            }
        }

        int dataCount = 0;
        bool inTable = false;

        for ( EVER ) {

            // If we've already been here, skip it
            if ( Attributes [ address ] & CODE ) {
                if (( Attributes [ address ] & CODE_START ) == 0 ) {
                    fprintf ( stderr, "Error - code doesn't begin with CODE_START!\n" );
                }
                break;
            }

            UINT8 opcode = Memory [ address ];

            if ( inTable && (( opcode < 0x40 ) || ( opcode > 0x7F ))) {
                break;
            }

            // Count FETCH instructions
            if (( opcode >= 0x88 ) && ( opcode < 0x89 )) {
                dataCount++;
            }

            int size = DisassembleGPL ( address, Memory + address, buffer );
            int badCode = 0;

            // Mark all bytes in this instruction as CODE
            Attributes [ address ] |= CODE_START;
            for ( int i = 0; ( badCode == 0 ) && ( i < size ); i++ ) {
                if ( Attributes [ address + i ] & CODE ) {
                    fprintf ( stderr, "Error parsing code - CODE attribute already set (%04X+%d)!\n", address, i );
                    badCode = i;
                    break;
                }
                if (( i == 0 )&& ( Attributes [ address + i ] & DATA )) {
                    fprintf ( stderr, "Error parsing code - DATA attribute set (%04X)!\n", address + i );
                    badCode = i + 1;
                    break;
                }
                if (( i != 0 ) && ( Attributes [ address + i ] & LABEL )) {
                    fprintf ( stderr, "Error parsing code - LABEL attribute set (%04X)!\n", address + i );
                    badCode = i + 1;
                    break;
                }
                if (( opcode != 0x08 ) && ( Attributes [ address + i ] & FMT_DATA )) {
                    fprintf ( stderr, "Error parsing code - FMT_DATA attribute set (%04X)!\n", address + i );
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

            // Look for BR or BS instructions
            if (( opcode >= 0x40 ) && ( opcode <= 0x7F )) {
                UINT16 newAddress = GetUINT16 ( Memory + address ) & ( BANK_SIZE - 1 );
                newAddress += address & BANK_MASK;
                PushAddress ( newAddress, TYPE_BRANCH, ( UINT16 ) -1 );
                // See if we're in a jump table ( ie: another BR follows )
                if ( opcode <= 0x5F ) {
                    UINT16 next = Memory [ address + 2 ];
                    if (( next >= 0x40 ) && ( next <= 0x5F )) {
                        inTable = true;
                    }
                }
            }

            // Look for RTN, RTNC, or EXIT instructions
            if (( opcode == 0x00 ) || ( opcode == 0x01 ) || ( opcode == 0x0B )) {
                break;
            }

            // Look for B or CALL instructions
            if (( opcode == 0x05 ) || ( opcode == 0x06 )) {
                UINT16 newAddress = GetUINT16 ( Memory + address + 1 );
                if ( opcode == 0x06 ) {
                    // We'll continue here after we've counted the # of FETCH commands
                    PushAddress ( address + size, TYPE_POST_CALL, newAddress );
                }
                // Now push the actual function/branch
                PushAddress ( newAddress, ( opcode == 0x06 ) ? TYPE_CALL : TYPE_BRANCH, ( UINT16 ) -1 );
                break;
            }

            // Look for FMT instructions
            if ( opcode == 0x08 ) {
                size += FindFormatData ( address + size );
            }

            address += size;
        }

        if (( top.reason == TYPE_CALL ) && ( dataCount != 0 )) {
            // Record the # of data bytes used by this subroutine
            DataCount [ top.address ] = dataCount;
        }
    }
}

UINT16 DumpByte ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpByte", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    char *tempPtr = buffer + sprintf ( buffer, "BYTE   " );

    for ( int i = 0; i < 8; i++ ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s DATA marked as %s\n", address, "Byte", "CODE" );
        }
        tempPtr += sprintf ( tempPtr, ">%02X", Memory [ address ]);
        if ( ++address > limit ) break;
        if ( Attributes [ address - 1 ] & DATA_SINGLE ) break;
        if ( Attributes [ address ] != 0 ) {
            if (( Attributes [ address ] & DATA_BYTE ) == 0 ) break;
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
    char *tempPtr = buffer + sprintf ( buffer, "DATA   " );

    for ( int i = 0; i < 8; i += 2 ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s DATA marked as %s\n", address, "Word", "CODE" );
        }
        tempPtr += sprintf ( tempPtr, ">%04X", GetUINT16 ( Memory + address ));
        address += 2;
        if ( Attributes [ address - 2 ] & DATA_SINGLE ) break;
        if ( address > limit ) break;
        if (( Attributes [ address ] & DATA_WORD ) == 0 ) break;
        if (( Attributes [ address ] & LABEL ) != 0 ) break;
        if ( i < 6 ) *tempPtr++ = ',';
    }

    return address;
}

UINT16 DumpString ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpString", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    int length = Memory [ address++ ];

    char *tempPtr = buffer + sprintf ( buffer, "STRI   '" );

    for ( int i = 0; i < length; i++ ) {
        *tempPtr++ = Memory [ address++ ];
        if ( address > limit ) break;
    }

    *tempPtr++ = '\'';
    *tempPtr = '\0';

    return address;
}

UINT16 DumpText ( UINT16 address, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpText", true );

    UINT16 limit = ( address & BANK_MASK ) + BANK_SIZE - 1;
    char *tempPtr = buffer + sprintf ( buffer, "BYTE   '" );

    for ( int i = 0; i < 16; i++ ) {
        if ( Attributes [ address ] & CODE ) {
            fprintf ( stderr, "**ERROR->%04X** %s bytes marked as %s\n", address, "Text", "CODE" );
        }
        *tempPtr++ = Memory [ address++ ];
        if ( address > limit ) break;
        if (( Attributes [ address ] & DATA_TEXT ) == 0 ) break;
        if (( Attributes [ address ] & LABEL ) != 0 ) break;
    }

    *tempPtr++ = '\'';
    *tempPtr = '\0';

    return address;
}

char *string ( UINT8 *src, int length )
{
    FUNCTION_ENTRY ( NULL, "string", true );

    static char buffer [ 128 ];
    char *dst = buffer;
    if ( length > 128 ) length = 128;
    while ( length-- ) {
        UINT8 ch = *src++;
        *dst++ = isprint ( ch ) ? ch : '.';
    }
    return buffer;
}

UINT16 DumpFMT ( UINT16 address, UINT16 *attributes, char *buffer )
{
    FUNCTION_ENTRY ( NULL, "DumpFMT", true );

    static int more = 1;
    if (( attributes [-1] & CODE_START ) != 0 ) more = 1;

    char *ptr = buffer + sprintf ( buffer, "%*.*s", more * 2, more * 2, "" );

    UINT8 code = Memory [ address++ ];
    int count = ( code & 0x1F ) + 1;
    switch ( code >> 5 ) {
        case 0 : // string accross ( HTEX / HSTR )
            sprintf ( ptr, "HTEX %d,'%*.*s'", count, count, count, string ( Memory + address, count ));
            address += count;
            break;
        case 1 : // string down ( VTEX / VSTR )
            sprintf ( ptr, "VTEX %d,'%*.*s'", count, count, count, string ( Memory + address, count ));
            address += count;
            break;
        case 2 : // repeat accross ( HCHA )
            sprintf ( ptr, isprint ( Memory [ address ]) ? "HCHA %d,'%c'" : "VCHA %d,>%02X", count, Memory [ address ]);
            address += 1;
            break;
        case 3 : // repeat down ( VCHA )
            sprintf ( ptr, isprint ( Memory [ address ]) ? "VCHA %d,'%c'" : "VCHA %d,>%02X", count, Memory [ address ]);
            address += 1;
            break;
        case 4 : // skip accross ( COL+ )
            sprintf ( ptr, "COL+ %d", count );
            break;
        case 5 : // skip down ( ROW+ )
            sprintf ( ptr, "ROW+ %d", count );
            break;
        case 6 : // repeat block ( FOR )
            sprintf ( ptr, "FOR %d", count );
            more++;
            break;
        case 7 : // special ( FEND / ROW / COL )
            switch ( code ) {
                case 0xFB :
                    sprintf ( ptr - 2, "FEND" );
                    more--;
                    break;
                case 0xFC :
                    sprintf ( ptr, "BIAS >%02X", Memory [ address++ ]);
                    break;
                case 0xFD :
                    sprintf ( ptr, "??BIAS >%02X", Memory [ address++ ]);
                    break;
                case 0xFE :
                    sprintf ( ptr, "YPT %d", Memory [ address++ ]);
                    break;
                case 0xFF :
                    sprintf ( ptr, "XPT %d", Memory [ address++ ]);
                    break;
                default :
                    sprintf ( ptr, "HSTR %d,<?>", count );
                    address++;
/*
    r5 = 0;
    UINT16 code = *gromPtr++;
    if (( code & 0x80 ) == 0 ) {
        code += 0x8300;
        if ( code == 0x837D ) {
            r10 = 0;
            SetCursorRead ();
            r0 = *vdp;
            if (( r0 & 0x0002 ) != 0 ) {
                r0 >>= 4;
            }
            *0x837D = r0;
            r0 = *code;
            if ( r5 == 0 ) {
                return * ( UINT8 * ) code;
            } else {
                return * ( UINT16 * ) code;
            }
        } else {
        }
    } else {
    }
*/
                    break;
            }
            break;
    }

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

    if (( Attributes [ address ] & CODE_START ) == 0 ) {
        fprintf ( stderr, "**ERROR->%04X** Instruction doesn't begin with CODE_START\n", address );
    }

    UINT16 size        = DisassembleGPL (( UINT16 ) address, Memory + address, tempBuffer );
    UINT16 nextAddress = address + size;

    bool ok = true;
    for ( int i = 0; i < size; i++ ) {
        if (( Attributes [ address + i ] & CODE ) == 0 ) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction not marked as CODE\n", address, i );
            ok = false;
        }
        if ( i && ( Attributes [ address + i ] & CODE_START )) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction marked with CODE_START\n", address, i );
            ok = false;
        }
        if ( Attributes [ address + i ] & DATA ) {
            fprintf ( stderr, "**ERROR->%04X+%d** Instruction marked as DATA\n", address, i );
        }
    }
    if ( ! ok ) {
        for ( int i = address; i < nextAddress; i++ ) {
            Attributes [ i ] = DATA_BYTE;
        }
        nextAddress = DumpByte ( address, buffer );
    } else {
        AddLabels ( buffer, tempBuffer + 5 );
        nextAddress = ( UINT16 ) ( address + size );
    }

    return nextAddress;
}

void DumpGrom ( UINT32 start, UINT32 end )
{
    FUNCTION_ENTRY ( NULL, "DumpGrom", true );

    fprintf ( outFile, "*\n" );
    fprintf ( outFile, "* Dump of GROM from %04X - %04X\n", start, end - 1 );
    fprintf ( outFile, "*\n" );

    while (( end > 0 ) && ( Labels [end-1] == 0 ) && ( Memory[end-1] == 0 )) {
        end--;
    }

    if ( end <= start ) {
        printf ( "No data found!\n" );
        return;
    }

    UINT32 address     = start;
    UINT32 lastAddress = start;

    printf ( "Dumping: %04X", address );

    bool equates = false;
    for ( UINT32 i = 0; i < 0x10000; i++ ) {
        if (( i >= start ) && ( i <= end )) continue;
        if ( Attributes [i] & LABEL ) {
            if ( equates == false ) {
                equates = true;
                fprintf ( outFile, "\n" );
            }
            fprintf ( outFile, "L%04X\t\tEQU    >%04X\n", i, i );
        }
    }

    fprintf ( outFile, "\n" );

    while ( address < end ) {

        if (( address & 0xFFF0 ) != lastAddress ) {
            lastAddress = address & 0xFFF0;
            printf ( "%04X", lastAddress );
        }

        char labelBuffer [ 32 ];
        if ( Labels [ address ] ) {
            strcpy ( labelBuffer, Labels [ address ] );
        } else if ( Attributes [ address ] & LABEL ) {
            sprintf ( labelBuffer, "L%04X", address );
        } else {
            strcpy ( labelBuffer, "" );
        }

        UINT16 nextAddress = ( UINT16 ) -1;
        char codeBuffer [ 256 ];

        if ( Attributes [ address ] & DATA ) {
            if ( Attributes [ address ] & DATA_STRING ) {
                nextAddress = DumpString (( UINT16 )  address, codeBuffer );
            } else if ( Attributes [ address ] & DATA_TEXT ) {
                nextAddress = DumpText (( UINT16 )  address, codeBuffer );
            } else if ( Attributes [ address ] & DATA_BYTE ) {
                nextAddress = DumpByte (( UINT16 ) address, codeBuffer );
            } else if ( Attributes [ address ] & DATA_WORD ) {
                nextAddress = DumpWord (( UINT16 )  address, codeBuffer );
            }
        } else if ( Attributes [ address ] & FMT_DATA ) {
            nextAddress = DumpFMT (( UINT16 )  address, &Attributes [ address ], codeBuffer );
        } else if ( Attributes [ address ] & CODE ) {
            nextAddress = DumpCode (( UINT16 ) address, codeBuffer );
        } else {
            nextAddress = DumpByte (( UINT16 ) address, codeBuffer );
        }

//        char dataBuffer [ 128 ];
//        char *ptr = dataBuffer;
//
//        UINT8 *temp  = Memory + address;
//        UINT32 index = address;
//        while ( index < nextAddress ) {
//            ptr += sprintf ( ptr, "%02X", *temp++ );
//            index++;
//        }
//        while ( index < address + 10 ) {
//            ptr += sprintf ( ptr, "  " );
//            index++;
//        }
//        ptr += sprintf ( ptr, " - \"" );
//        while ( gromPtr != temp ) {
//            UINT8 ch = *gromPtr++;
//            ptr += sprintf ( ptr, "%c", isprint ( ch ) ? ch : '.' );
//        }
//        sprintf ( ptr, "\"" );

        int tabs;
        tabs = 2 - strlen ( labelBuffer ) / 8;
        DBG_ASSERT ( tabs <= 5 );
        fprintf ( outFile, "%s%*.*s", labelBuffer, tabs, tabs, "\t\t\t\t\t" );

        tabs = 7 - strlen ( codeBuffer ) / 8;
        DBG_ASSERT ( tabs <= 7 );
        fprintf ( outFile, "%s%*.*s", codeBuffer, tabs, tabs, "\t\t\t\t\t\t\t" );

//        tabs = 4 - ( strlen ( dataBuffer ) + 2 ) / 8;
//        DBG_ASSERT ( tabs <= 5 );
//        fprintf ( outFile, "* %s%*.*s", dataBuffer, tabs, tabs, "\t\t\t\t\t" );

//        fprintf ( outFile, "* %04X\n", address );
        fprintf ( outFile, "* %04X -", address );
        for ( UINT16 i = address; i < nextAddress; i++ ) {
            fprintf ( outFile, " %02X", Memory [ i ] );
        }
        fprintf ( outFile, "\n" );

        if (( Attributes [ address ] & CODE ) &&
            (( Memory [ address ] == 0x00 ) || ( Memory [ address ] == 0x01 ) || ( Memory [ address ] == 0x0B ))) {
            fprintf ( outFile, "------------------------------------------------\n" );
        }

        address = nextAddress;
    }

    printf ( "complete\n" );
}

bool g_LinePresent;
char g_LineBuffer [256];

void GetLine ( char *buffer, int size, FILE *file )
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

void UnGetLine ( char *buffer )
{
    FUNCTION_ENTRY ( NULL, "UnGetLine", true );

    strcpy ( g_LineBuffer, buffer );
    g_LinePresent = true;
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
        PushAddress ( address, TYPE_BRANCH, false );

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
        switch ( sscanf ( ptr, "%04X", &address )) {
            case 1 :
                break;
            default :
                fprintf ( stderr, "Error parsing line '%s'", buffer );
                break;
        }

        if ( symLength != 0 ) {
            Labels [ address ] = strdup ( symbol );
        }
        PushAddress ( address, TYPE_CALL, false );

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

        Attributes [address] |= LABEL;
        for ( unsigned int i = 0; i < count; i++ ) {
            Attributes [address+i] |= attr;
        }
    }
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
    }

    fclose ( file );
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

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: dumpgrom [options] file\n" );
    fprintf ( stdout, "\n" );
}

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

    for ( unsigned i = 0; i < SIZE ( ctg.GromMemory ); i++ ) {
        if ( ctg.GromMemory[i].NumBanks > 0 ) {
            if ( lo == -1 ) lo = i;
            hi = i + 1;
            UINT8 *ptr = ctg.GromMemory[i].Bank[0].Data;
            memcpy (( char * ) &Memory [ i * BANK_SIZE ], ( char * ) ptr, BANK_SIZE );
            for ( unsigned j = 0; j < BANK_SIZE; j++ ) {
                Attributes [ i * BANK_SIZE + j ] |= PRESENT;
            }
        }
    }

    *rangeLo = lo * BANK_SIZE;
    *rangeHi = hi * BANK_SIZE;

    return strdup ( ctg.Title ());
}

int main ( int argc, char *argv[] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    sRange range        = { 0x00000, 0x10000 };

    sOption optList [] = {
        {  0,  "range*=lo-hi",  OPT_NONE,      0,    &range,         ParseRange, "Only dump the indicate range" },
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "TI-99/4A GROM Disassembler\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    Init ();

    char fileName [ 256 ];

    // First, look for the name without any modifications
    const char *validName = LocateFile ( argv [index] );
    if ( validName == NULL ) {
        // Look for the file in the 'normal' places for .ctg files
        validName = LocateFile ( argv [index], "cartridges" );
        if ( validName == NULL ) {
            validName = LocateFile ( argv [index], "roms" );
            if ( validName == NULL ) {
                sprintf ( fileName, "%s.%s", argv [index], "ctg" );
                validName = LocateFile ( fileName, "cartridges" );
                if ( validName == NULL ) {
                    validName = LocateFile ( fileName, "roms" );
                }
            }
        }
    }

    if ( validName == NULL ) {
        fprintf ( stderr, "Unable to locate file \"%s\"\n", argv [index] );
        return -1;
    }

    const char *moduleName = LoadCartridge ( validName, &range.lo, &range.hi );
    if ( moduleName == NULL ) return -1;

    const char *_start = strrchr ( validName, FILE_SEPERATOR );
    _start = ( _start == NULL ) ? validName : _start + 1;

    char baseName [ 256 ];
    strcpy ( baseName, _start );

    char *ptr = strrchr ( baseName, '.' );
    if ( ptr != NULL ) *ptr = '\0';

    sprintf ( fileName, "%s.%s", baseName, "cfg" );
    ReadConfig ( fileName );

    sprintf ( fileName, "%s.%s", baseName, "gpl" );
    outFile = fopen ( fileName, "wt" );
    if ( outFile == NULL ) {
        fprintf ( stderr, "Unable to open file \"%s\" for writing\n", fileName );
        free (( void * ) moduleName );
        return -1;
    }

    DataCount [ 0x0010 ] = 1;

    bool headerFound = false;
    fprintf ( stdout, "Headers located:" );
    for ( unsigned i = 0; i < 8; i++ ) {
        if (( Attributes [ i * BANK_SIZE ] & PRESENT ) != 0 ) {
            if ( InterpretHeader ( i * BANK_SIZE ) == true ) {
                headerFound = true;
                fprintf ( stdout, " %04X", i * BANK_SIZE );
            }
        }
    }
    fprintf ( stdout, ( headerFound == false ) ? " -NONE-\n\n" : "\n\n" );

    TraceCode ();

    DumpGrom ( range.lo, range.hi );

    fclose ( outFile );

    free (( void * ) moduleName );

    return 0;
}
