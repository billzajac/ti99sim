//
// File:        convert.cpp
// Date:        09-May-1995
// Programmer:  Marc Rousseau
//
// Description: This programs will convert a .lst or .dat file to a .ctg file
//
// Copyright (c) 1995-2004 Marc Rousseau, All Rights Reserved.
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
#if ! defined ( _MSC_VER )
    #include <dirent.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "option.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

#if defined ( OS_OS2 ) || defined ( OS_WINDOWS )
    const char MARKER_CHAR = '\x10';
#else
    const char MARKER_CHAR = '*';
#endif

enum eFileType {
    LST,
    ROM,
    HEX,
    GK,
    CTG,
    PIF
};

struct sGromHeader {
    // This is only valid AFTER bytes have been swapped !!!
    UINT8    Valid;
    UINT8    Version;
    UINT8    NoApplications;
    UINT8    reserved1;
    UINT16   PowerUpHeader;
    UINT16   ApplicationHeader;
    UINT16   DSR_Header;
    UINT16   SubprogramHeader;
    UINT16   InterruptHeader;
    UINT16   reserved2;

    sGromHeader ( UINT8 *ptr );
};

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

sGromHeader::sGromHeader ( UINT8 *ptr ) :
    Valid ( 0 ),
    Version ( 0 ),
    NoApplications ( 0 ),
    reserved1 ( 0 ),
    PowerUpHeader ( 0 ),
    ApplicationHeader ( 0 ),
    DSR_Header ( 0 ),
    SubprogramHeader ( 0 ),
    InterruptHeader ( 0 ),
    reserved2 ( 0 )
{
    FUNCTION_ENTRY ( this, "sGromHeader ctor", true );

    sGromHeader *rawHdr = ( sGromHeader * ) ptr;

    Valid             = rawHdr->Valid;
    Version           = rawHdr->Version;
    NoApplications    = rawHdr->NoApplications;
    reserved1         = rawHdr->reserved1;

    PowerUpHeader     = GetUINT16 ( &rawHdr->PowerUpHeader );
    ApplicationHeader = GetUINT16 ( &rawHdr->ApplicationHeader );
    DSR_Header        = GetUINT16 ( &rawHdr->DSR_Header );
    SubprogramHeader  = GetUINT16 ( &rawHdr->SubprogramHeader );
    InterruptHeader   = GetUINT16 ( &rawHdr->InterruptHeader );
    reserved2         = GetUINT16 ( &rawHdr->reserved2 );
}

void FindName ( cCartridge &cart )
{
    FUNCTION_ENTRY ( NULL, "FindName", true );

    if ( cart.Title () != NULL ) return;

    printf ( "Found the following names:\n" );

    char *title = NULL;

    // Search GROM for headers
    for ( unsigned i = 0; i < SIZE ( cart.GromMemory ); i++ ) {
        sMemoryRegion *memory = &cart.GromMemory[i];
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            UINT8 *data = memory->Bank[0].Data;
            sGromHeader hdr ( data );
            if ( hdr.Valid != 0xAA ) continue;
            int addr = i * GROM_BANK_SIZE;
            if ( hdr.ApplicationHeader == 0 ) continue;
            int appHdr = hdr.ApplicationHeader;
            while ( appHdr ) {
                appHdr -= addr;
                if (( appHdr < 0 ) || ( appHdr > GROM_BANK_SIZE )) break;
                UINT8 length = data [ appHdr + 4 ];
                UINT8 *name = &data [ appHdr + 5 ];
                printf ( " %c %*.*s\n", title ? ' ' : MARKER_CHAR, length, length, name );
                appHdr = ( data [ appHdr ] << 8 ) + data [ appHdr + 1 ];
                if ( title == NULL ) {
                    title = ( char * ) malloc ( length + 1 );
                    sprintf ( title, "%*.*s", length, length, name );
                }
            }
        }
    }

    // Search ROM for headers
    for ( unsigned i = 0; i < SIZE ( cart.CpuMemory ); i++ ) {
        sMemoryRegion *memory = &cart.CpuMemory[i];
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            UINT8 *data = memory->Bank[0].Data;
            if ( data == NULL ) {
                DBG_ERROR ( "Invalid memory - bank "<< j << " is missing @>" << hex << ( UINT16 ) ROM_BANK_SIZE );
                continue;
            }
            sGromHeader hdr ( data );
            if ( hdr.Valid != 0xAA ) continue;
            int addr = i * ROM_BANK_SIZE;
            if ( hdr.ApplicationHeader == 0 ) continue;
            int appHdr = hdr.ApplicationHeader;
            while ( appHdr ) {
                appHdr -= addr;
                if ( appHdr > ROM_BANK_SIZE ) break;
                UINT8 length = data [ appHdr + 4 ];
                UINT8 *name = &data [ appHdr + 5 ];
                printf ( " %c %*.*s\n", title ? ' ' : MARKER_CHAR, length, length, name );
                appHdr = ( data [ appHdr ] << 8 ) + data [ appHdr + 1 ];
                if ( title == NULL ) {
                    title = ( char * ) malloc ( length + 1 );
                    sprintf ( title, "%*.*s", length, length, name );
                }
            }
        }
    }

    cart.SetTitle ( title );

    printf ( "\n" );
}

void ShowSummary ( const cCartridge &cartridge )
{
    FUNCTION_ENTRY ( NULL, "ShowSummary", true );

    printf ( "\nModule Summary:\n" );

    printf ( "  Title: %s\n", cartridge.Title ());

    UINT16 cru = cartridge.GetCRU ();
    if ( cru != 0 ) {
        printf ( "    CRU: %04X\n", cru );
    }

    printf ( "  GROMS: " );
    int groms = 0;
    for ( unsigned i = 0; i < SIZE ( cartridge.GromMemory ); i++ ) {
        if ( cartridge.GromMemory[i].NumBanks > 0 ) {
            printf ( "%d ", i );
            groms++;
        }
    }
    printf ( groms ? "\n" : "None\n" );

    if ( cartridge.CpuMemory [0].NumBanks > 0 ) printf ( "  Operating System ROM\n" );

    for ( unsigned i = 2; i < SIZE ( cartridge.CpuMemory ); i++ ) {
        if ( cartridge.CpuMemory[i].NumBanks > 0 ) {
            int banks = cartridge.CpuMemory[i].NumBanks;
            int type  = cartridge.CpuMemory[i].Bank[0].Type;
            printf ( "  %d bank%s of %s at %04X\n", banks, ( banks > 1 ) ? "s" : "", ( type == BANK_ROM ) ? "ROM" : "RAM" , i * 0x1000 );
        }
    }

    printf ( "\n" );
}

//----------------------------------------------------------------------------
//
// Create a module from a .lst/.dat listing file
//
// Valid Regions:
//
//    ROM   - CPU memory
//    GROM  - GROM memory
//
// The types of sections that can be specified are:
//
//    ROM   - Read-Only memory
//    RAM   - Read/Write memory
//    RAMB  - Battery backed Read/Write memory
//
// ROM/RAM banks are 4096 (0x1000) bytes
// GROM/GRAM banks are 8192 (0x2000) bytes
//
//----------------------------------------------------------------------------
const char *types[6] = { "RAM", "ROM", "RAMB" };

int ConvertDigit ( char digit )
{
    FUNCTION_ENTRY ( NULL, "ConvertDigit", true );

    int number = digit - '0';
    if ( number > 9 ) number -= ( 'A' - '0' ) - 10;
    return number;
}

char *GetAddress ( char *ptr, UINT32 *address )
{
    FUNCTION_ENTRY ( NULL, "GetAddress", true );

    *address = 0;

    if ( ptr == NULL ) return NULL;

    while ( isxdigit ( *ptr )) {
        *address <<= 4;
        *address |= ConvertDigit ( *ptr++ );
    }

    while (( *ptr != '\0' ) && ! isspace ( *ptr )) ptr++;

    return ( *ptr != '\0' ) ? ptr : NULL;
}

static int LineNumber = 0;
static const char *FileName;

bool ReadLine ( FILE *file, char *buffer, int length )
{
    FUNCTION_ENTRY ( NULL, "ReadLine", true );

    *buffer = '\0';

    do {
        if ( fgets ( buffer, length, file ) == NULL ) {
            return false;
        }
        LineNumber++;
        while (( *buffer != '\0' ) && ( isspace ( *buffer ))) buffer++;
    } while ( *buffer == '*' );

    return true;
}

void ReadBank ( FILE *file, UINT8 *buffer, int size )
{
    FUNCTION_ENTRY ( NULL, "ReadBank", true );

    char line [ 256 ];

    int mask = size - 1;

    unsigned max = ( unsigned ) -1;

    for ( int j = 0; j < size; ) {

        if ( ReadLine ( file, line, sizeof ( line )) == false ) {
            fprintf ( stderr, "\n%s:%d: Unexpected end of file\n", FileName, LineNumber );
            exit ( -1 );
        }

        UINT32 address;
        char *src = GetAddress ( line, &address );
        if ( src == NULL ) break;

        fprintf ( stdout, "%04X", address );
        fflush ( stdout );

        UINT8 *dest = &buffer [ address & mask ];

        int x = 0;
        while (( *src != '\0' ) && (( unsigned ) x < max )) {
            while (( *src == ' ' ) || ( *src == '-' )) src++;
            if ( isxdigit ( src [0] ) && isxdigit ( src [1] )) {
                int hi = ConvertDigit ( *src++ );
                int lo = ConvertDigit ( *src++ );
                if ( isxdigit ( *src )) break;
                *dest++ = ( UINT8 ) (( hi << 4 ) | lo );
                if ( ++j == size ) break;
                x++;
            } else {
                break;
            }
        }
        if (( max == ( unsigned ) -1 ) && ( x > 0 )) max = x;
    }

    fprintf ( stdout, "    " );
    fflush ( stdout );
}

void ReadFile ( const char *filename, cCartridge &cartridge )
{
    FUNCTION_ENTRY ( NULL, "ReadFile", true );

    FILE *file = fopen ( filename, "rt" );
    if ( file == NULL ) {
        fprintf ( stderr, "ERROR: Unable to open file \"%s\"\n", filename );
        exit ( -1 );
    }

    FileName = filename;
    LineNumber = 0;

    char line [ 256 ], temp [ 10 ];

    if ( ReadLine ( file, line, sizeof ( line )) == false ) {
        fprintf ( stderr, "ERROR: %s:%d: File is empty!\n", FileName, LineNumber );
        exit ( -1 );
    }

    // Look for a module title (optional)
    if ( line[0] == '[' ) {
        line [ strlen ( line ) - 2 ] = '\0';
        cartridge.SetTitle ( &line[1] );
        if ( ReadLine ( file, line, sizeof ( line )) == false ) {
            fprintf ( stderr, "ERROR:  %s:%d: File is invalid - must have at least one bank of ROM/GROM\n", FileName, LineNumber );
            exit ( -1 );
        }
    }

    // Look for a CRU address (optional)
    if ( strnicmp ( &line [2], "CRU", 3 ) == 0 ) {
        unsigned int base;
        sscanf ( &line [2], "%s = %x", temp, &base );
        if ( base != 0 ) {
            cartridge.SetCRU ( base );
        }
        if ( ReadLine ( file, line, sizeof ( line )) == false ) {
            fprintf ( stderr, "ERROR: %s:%d: File is invalid - must have at least one bank of ROM/GROM\n", FileName, LineNumber );
            exit ( -1 );
        }
    }

    int errors = 0;

    for ( EVER ) {

        if ( line[0] != ';' ) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Expected line beginning with ';'.\n", FileName, LineNumber );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        int index;
        char type [10];
        sscanf ( &line[2], "%s %d", type, &index );

        // Get the next memory type (ROM or GROM) and index
        if (( strcmp ( type, "ROM" ) != 0 ) && ( strcmp ( type, "GROM" ) != 0 )) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Invalid memory index '%s' - expected either 'ROM' or 'GROM'.\n", FileName, LineNumber, type );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        int size = ( type [0] == 'G' ) ? GROM_BANK_SIZE : ROM_BANK_SIZE;
        int maxIndex = ( type [0] == 'G' ) ? SIZE ( cartridge.GromMemory ) : SIZE ( cartridge.CpuMemory );
        if ( index > maxIndex ) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Invalid %s index (%d) indicated.\n", FileName, LineNumber, type, index );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }
        sMemoryRegion *memory = ( type [0] == 'G' ) ? &cartridge.GromMemory [index] : &cartridge.CpuMemory [index];

        if ( ReadLine ( file, line, sizeof ( line )) == false ) break;

    READ_BANK:

        if ( line [0] != ';' ) {
            errors += 1;
            fprintf ( stderr, "%s:%d: Expected line beginning with ';'.\n", FileName, LineNumber );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        int bank;
        if ( sscanf ( &line[2], "%s %d - %s", temp, &bank, type ) != 3 ) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Syntax error.\n", FileName, LineNumber );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        if ( strcmp ( temp, "BANK" ) != 0 ) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Syntax error - expected BANK statement.\n", FileName, LineNumber );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        if (( strcmp ( type, "ROM" ) != 0 ) && ( strcmp ( type, "RAM" ) != 0 ) && ( strcmp ( type, "RAMB" ) != 0 )) {
            errors += 1;
            fprintf ( stderr, "WARNING: %s:%d: Invalid memory type '%s' - expected either 'ROM', 'RAM', or 'RAMB'.\n", FileName, LineNumber, type );
            if ( ReadLine ( file, line, sizeof ( line )) == false ) break;
            continue;
        }

        BANK_TYPE_E memoryType = ( type [3] == 'B' ) ? BANK_BATTERY_BACKED :
                                   ( type [1] == 'O' ) ? BANK_ROM : BANK_RAM;

        if ( memory->NumBanks <= bank ) {
            memory->NumBanks = bank + 1;
        }

        UINT8 *ptr = new UINT8 [ size ];
        memset ( ptr, 0, size );

        memory->Bank [bank].Type = memoryType;
        memory->Bank [bank].Data = ptr;

        if ( memoryType == BANK_ROM ) {
            fprintf ( stdout, "\nReading %-4s %2d bank %d:     ", ( size == ROM_BANK_SIZE ) ? "ROM" : "GROM", index, bank );
            fflush ( stdout );
            ReadBank ( file, memory->Bank [bank].Data, size );
        }

        if ( ReadLine ( file, line, sizeof ( line )) == false ) break;

        if ( strnicmp ( &line [2], "BANK", 4 ) == 0 ) goto READ_BANK;

    }

    printf ( "\n" );

    fclose ( file );

    if ( errors > 0 ) {
        exit ( -1 );
    }
}

//----------------------------------------------------------------------------
//
// Read raw binary file(s)
//
//----------------------------------------------------------------------------
bool ReadHex ( const char *gromName, const char *romName1, const char *romName2, cCartridge &cartridge, bool isDSR )
{
    FUNCTION_ENTRY ( NULL, "ReadHex", true );

    bool foundData = false;

    // Read GROM
    cFile *file = cFile::Open ( gromName, "disks" );
    if ( file != NULL ) {

        foundData = true;

        int bytesLeft = file->FileSize ();
        int region    = 3;

        do {
            sMemoryRegion &memory = cartridge.GromMemory[region++];
            memory.NumBanks     = 1;
            memory.Bank[0].Type = BANK_ROM;
            memory.Bank[0].Data = new UINT8 [ GROM_BANK_SIZE ];
            for ( int offset = 0; offset < GROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE ) {
                int count = file->ReadRecord ( memory.Bank[0].Data + offset, DEFAULT_SECTOR_SIZE );
                bytesLeft -= count;
            }
        } while ( bytesLeft > 0 );

        file->Release ( NULL );
    }

    // Read RAM 0
    file = cFile::Open ( romName1, "disks" );
    if ( file != NULL ) {

        foundData = true;
        int baseIndex = ( isDSR == true ) ? 4 : 6;

        for ( int i = 0; i < 2; i++ ) {
            sMemoryRegion &memory = cartridge.CpuMemory[baseIndex + i];
            memory.NumBanks     = 1;
            memory.Bank[0].Type = BANK_ROM;
            memory.Bank[0].Data = new UINT8 [ ROM_BANK_SIZE ];
            for ( int offset = 0; offset < ROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE ) {
                file->ReadRecord ( memory.Bank[0].Data + offset, DEFAULT_SECTOR_SIZE );
            }
        }

        file->Release ( NULL );

        // Read RAM 1
        file = cFile::Open ( romName2, "disks" );
        if ( file != NULL ) {
            for ( int i = 0; i < 2; i++ ) {
                sMemoryRegion &memory = cartridge.CpuMemory[baseIndex + i];
                memory.NumBanks     = 2;
                memory.Bank[1].Type = BANK_ROM;
                memory.Bank[1].Data = new UINT8 [ ROM_BANK_SIZE ];
                for ( int offset = 0; offset < ROM_BANK_SIZE; offset += DEFAULT_SECTOR_SIZE ) {
                    file->ReadRecord ( memory.Bank[0].Data + offset, DEFAULT_SECTOR_SIZE );
                }
            }

            file->Release ( NULL );
        }

    }

    return foundData;
}

//----------------------------------------------------------------------------
//
// Read V9T9 style .hex/.bin files given a 'base' file name.
//
// A search is done first for V6.0 style names using the specified extension
//   i.e. Given foo.bin -> foog.bin, fooc.bin, food.bin
// If no match is found, a search for earlier naming conventions is made
//   i.e. Given foo.hex -> foo(g).hex, fooc0.hex, fooc1.hex
//
//----------------------------------------------------------------------------
void ReadHex ( const char *fileName, cCartridge &cartridge, bool isDSR )
{
    FUNCTION_ENTRY ( NULL, "ReadHex", true );

    static const char *gromNames [] = { "%sg.%s", "%sG.%s", "%s.%s" };
    static const char *romNames1 [] = { "%sc.%s", "%sC.%s", "%sc0.%s", "%sC0.%s" };
    static const char *romNames2 [] = { "%sd.%s", "%sD.%s", "%sc1.%s", "%sC1.%s" };

    FILE *file = NULL;

    int warnings = 0;

    if ( isDSR == true ) {

        // Read DSR ROM
        file = fopen ( fileName, "rb" );
        if ( file == NULL ) return;

        for ( int i = 4; i < 6; i++ ) {
            sMemoryRegion &memory = cartridge.CpuMemory[i];
            memory.NumBanks = 1;
            memory.Bank[0].Type = BANK_ROM;
            memory.Bank[0].Data = new UINT8 [ ROM_BANK_SIZE ];
            int read = fread ( memory.Bank[0].Data, 1, ROM_BANK_SIZE, file );
            if ( read != ROM_BANK_SIZE ) {
                if ( warnings++ == 0 ) fprintf ( stderr, "\n" );
                fprintf ( stderr, "WARNING: Error reading from DSR GROM file \"%s\" (read %d bytes, expecting %d bytes) \n", fileName, read, ROM_BANK_SIZE );
                memory.NumBanks = 0;
            }
        }

        fclose ( file );

        return;
    }

    char filename [512], name [512], ext [4];

    strcpy ( filename, fileName );

    // Truncate the filename to the 'base' filename
    size_t len = strlen ( filename );
    strcpy ( ext, &filename [len-3]);
    filename [len-4] = '\0';

    // Read GROM
    for ( unsigned i = 0; i < SIZE ( gromNames ); i++ ) {
        sprintf ( name, gromNames [i], filename, ext );
        file = fopen ( name, "rb" );
        if ( file != NULL ) break;
    }

    int region = 3;
    if ( file != NULL ) {
        int ch = getc ( file );
        while ( ! feof ( file )) {
            ungetc ( ch, file );
            sMemoryRegion &memory = cartridge.GromMemory[region++];
            memory.NumBanks = 1;
            memory.Bank[0].Type = BANK_ROM;
            memory.Bank[0].Data = new UINT8 [ GROM_BANK_SIZE ];
            int read = fread ( memory.Bank[0].Data, 1, GROM_BANK_SIZE, file );
            if ( read != GROM_BANK_SIZE ) {
                if ( warnings++ == 0 ) fprintf ( stderr, "\n" );
                fprintf ( stderr, "WARNING: Error reading from GROM file \"%s\" (read %d bytes, expecting %d bytes) \n", name, read, GROM_BANK_SIZE );
            }
            ch = getc ( file );
        }
        fclose ( file );
    }

    // Read RAM 0
    for ( unsigned i = 0; i < SIZE ( romNames1 ); i++ ) {
        sprintf ( name, romNames1 [i], filename, ext );
        file = fopen ( name, "rb" );
        if ( file != NULL ) break;
    }
    if ( file == NULL ) {
        // Print an error if we couldn't load any files
        if ( region == 3 ) {
            fprintf ( stderr, "ERROR: Unable to open ROM file \"%s.%s\"\n", filename, ext );
            exit ( -1 );
        }
        if ( warnings != 0 ) fprintf ( stderr, "\n" );
        return;
    }

    for ( int i = 6; i < 8; i++ ) {
        sMemoryRegion &memory = cartridge.CpuMemory[i];
        memory.NumBanks = 1;
        memory.Bank[0].Type = BANK_ROM;
        memory.Bank[0].Data = new UINT8 [ ROM_BANK_SIZE ];
        int read = fread ( memory.Bank[0].Data, 1, ROM_BANK_SIZE, file );
        if ( read != ROM_BANK_SIZE ) {
            if ( warnings++ == 0 ) fprintf ( stderr, "\n" );
            fprintf ( stderr, "WARNING: Error reading from ROM file \"%s\" (read %d bytes, expecting %d bytes) \n", name, read, ROM_BANK_SIZE );
        }
    }

    fclose ( file );

    // Read RAM 1
    for ( unsigned i = 0; i < SIZE ( romNames2 ); i++ ) {
        sprintf ( name, romNames2 [i], filename, ext );
        file = fopen ( name, "rb" );
        if ( file != NULL ) break;
    }
    if ( file == NULL ) {
        if ( warnings != 0 ) fprintf ( stderr, "\n" );
        return;
    }

    for ( int i = 6; i < 8; i++ ) {
        sMemoryRegion &memory = cartridge.CpuMemory[i];
        memory.NumBanks = 2;
        memory.Bank[1].Type = BANK_ROM;
        memory.Bank[1].Data = new UINT8 [ ROM_BANK_SIZE ];
        int read = fread ( memory.Bank[1].Data, 1, ROM_BANK_SIZE, file );
        if ( read != ROM_BANK_SIZE ) {
            if ( warnings++ == 0 ) fprintf ( stderr, "\n" );
            fprintf ( stderr, "WARNING: Error reading from ROM file \"%s\" (read %d bytes, expecting %d bytes) \n", name, read, ROM_BANK_SIZE );
        }
    }
    fclose ( file );

    if ( warnings != 0 ) fprintf ( stderr, "\n" );
}

bool FindFile ( char *filename )
{
#if defined ( _MSC_VER )
    FILE *file =  fopen ( filename, "r+" );
    if ( file != NULL )
    {
        fclose ( file );
        return true;
    }
    return false;
#else
    DIR *dir = opendir ( "." );
    dirent *dp = readdir ( dir );
    while ( dp != NULL )
    {
        if( !(dp->d_type & DT_DIR ) && !stricmp ( filename, dp->d_name ))
        {
            strcpy ( filename, dp->d_name );
            closedir ( dir );
            return true;
        }
        dp = readdir ( dir );
    }

    closedir ( dir );
    return false;
#endif
}

//----------------------------------------------------------------------------
//  Read Microsoft Program Information Files
//----------------------------------------------------------------------------
void ReadProgramInformation ( const char *fileName, cCartridge &cartridge )
{
    FILE *file = fopen ( fileName, "rb" );
    if ( file == NULL ) {
        fprintf ( stderr, "ERROR: Unable to open file \"%s\"\n", fileName );
        return;
    }
    char buffer [4096];
    // Jump to the Parameters string of the Basic section
    fseek ( file, 0xA5, SEEK_SET );
    if ( fgets ( buffer, sizeof ( buffer ), file ) == NULL )
    {
        fclose ( file );
        return;
    }
    fclose ( file );

    char romName [4096];

    char *ptr = strstr ( buffer, "-cart " );
    while ( ptr != NULL )
    {
        char *name = ptr + strlen ( "-cart " );
        while ( *name && isspace ( *name )) name++;
        char *end = name;
        while ( *end && ! isspace ( *end )) end++;
        int length = end - name;
        snprintf ( romName, 4096, "%*.*s", length, length, name );
        // Find and correct the case of the filename
        if ( ! FindFile ( romName ))
        {
            return;
        }
        if ( stricmp ( romName, "blankg.bin" ) != 0 )
        {
            char *ext = strchr ( romName, '.' );
            memcpy ( ext - 1, ext, strlen ( ext ) + 1 );
            ReadHex ( romName, cartridge, false );
            return;
        }
        ptr = strstr ( end, "-cart " );
    }
}

//----------------------------------------------------------------------------
//  Read V9T9 style CPU & GPL console ROMS and make a 'special' cartridge
//----------------------------------------------------------------------------
void ReadConsole ( const char *fileName, cCartridge &cartridge )
{
    FUNCTION_ENTRY ( NULL, "ReadConsole", true );

    static const char *romNames [] = { "", "%sticpu.%s", "%s994arom.%s" };
    static const char *gromNames [] = { "", "%stigpl.%s", "%s994agrom.%s" };

    char filename [512], name [ 512 ];

    strcpy ( filename, fileName );

    const char *base = "";

    // Truncate the filename to the 'base' filename
    size_t type = 0, len = strlen ( filename ), max = len;
    const char *ext = &filename [len-3];

    if (( len >= 6 ) && ( strnicmp ( filename + len - 6, "ti.", 3 ) == 0 )) {
        max = 6;
        type = 1;
    }
    if (( len >= 8 ) && ( strnicmp ( filename + len - 8, "994a.", 5 ) == 0 )) {
        max = 8;
        type = 2;
    }

    if ( len > max ) {
        base = filename;
        filename [ len - max ] = '\0';
    }

    sprintf ( name, romNames [type], base, ext );
    ReadHex ( name, cartridge, false );

    // Move GROM 3 to ROMs 0+1
    sMemoryRegion &memory = cartridge.GromMemory[3];
    cartridge.CpuMemory[0] = memory;
    cartridge.CpuMemory[1] = memory;
    cartridge.CpuMemory[1].Bank[0].Data = new UINT8 [ ROM_BANK_SIZE ];
    memcpy ( cartridge.CpuMemory[1].Bank[0].Data, cartridge.CpuMemory[0].Bank[0].Data + ROM_BANK_SIZE, ROM_BANK_SIZE );

    sprintf ( name, gromNames [type], base, ext );
    ReadHex ( name, cartridge, false );

    // Move GROMs 3+4+5 to GROMs 0+1+2 and
    memcpy ( &cartridge.GromMemory[0], &cartridge.GromMemory[3], 3 * sizeof ( sMemoryRegion ));

    // Use GROM 6 to zero out GROMs 3+4+5
    memory = cartridge.GromMemory[6];
    cartridge.GromMemory[3] = memory;
    cartridge.GromMemory[4] = memory;
    cartridge.GromMemory[5] = memory;

    // Create banks for 32K Memory Expansion and scratchpad RAM
    int ramBanks [] = { 0x02, 0x03, 8, 9, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

    for ( unsigned i = 0; i < SIZE ( ramBanks ); i++ ) {
        cartridge.CpuMemory[ramBanks[i]].NumBanks     = 1;
        cartridge.CpuMemory[ramBanks[i]].Bank[0].Type = BANK_RAM;
    }

    // Set the name and save it to the expected filename
    cartridge.SetTitle ( "TI-99/4A Console" );
    cartridge.SaveImage ( "TI-994A.ctg" );

    ShowSummary ( cartridge );
}

//----------------------------------------------------------------------------
//
//    Gram Kracker: Loader/Saver Order and Header bytes
//
//    Byte        Description
//     00         More to load flag
//                  FF = More to load
//                  80 = Load UTIL Option next
//                  00 = Last file to load
//     01         What Gram Chip or Ram Bank
//                  01 = Grom/Gram 0 g0000
//                  02 = Grom/Gram 1 g2000
//                  03 = Grom/Gram 2 g4000
//                  04 = Grom/Gram 3 g6000
//                  05 = Grom/Gram 4 g8000
//                  06 = Grom/Gram 5 gA000
//                  07 = Grom/Gram 6 gC000
//                  08 = Grom/Gram 7 gE000
//                  09 = Rom/Ram Bank 1 c6000
//                  0A = Rom/Ram Bank 2 c6000
//            00 or FF = Program Image - load to Memory Expansion
//     02         0000 = Number of bytes to load
//     04         0000 = Address to start loading at
//
//    Try to support the following file formats:
//      *.PROG - 'native1' format (Actual GK file)
//      *.grm  - 'native2' format ('Fake' GK file - different naming scheme)
//      *      - v9t9 FIAD
//
//----------------------------------------------------------------------------

bool ReadGK ( const char *baseFilename, cCartridge &cart )
{
    FUNCTION_ENTRY ( NULL, "ReadGK", true );

    enum {
        FORMAT_NATIVE1,
        FORMAT_NATIVE2,
        FORMAT_FIAD
    } type = FORMAT_FIAD;

    char name [80], ext [8];
    strcpy ( name, baseFilename );
    memset ( ext, 0, sizeof ( ext ));
    char *ptr = strrchr ( name, '.' );
    char *start = strrchr ( name, FILE_SEPERATOR );
    start = ( start == NULL ) ? name : start + 1;

    char filename [80];
    strcpy ( filename, baseFilename );

    if (( ptr != NULL ) && ( ptr < start )) {
        ptr = NULL;
    }

    if ( ptr != NULL ) {
        strcpy ( ext, ptr );
        *ptr = '\0';
        if ( stricmp ( ext, ".PROG" ) == 0 ) {
            type = FORMAT_NATIVE1;
        } else if ( stricmp ( ext, ".grm" ) == 0 ) {
            type = FORMAT_NATIVE2;
            char *lastChar = start + strlen ( start ) - 1;
            if ( *lastChar != '1' ) {
                sprintf ( filename, "%s%d%s", name, 1, ext );
            } else {
                *lastChar = '\0';
            }
        } else {
            fprintf ( stderr, "File \"%s\" is not recognized as a Gram Kracker file\n", baseFilename );
            return false;
        }
    }

    start [9] = '\0';

    int more = 0xFF;

    for ( int i = ( type == FORMAT_NATIVE2 ) ? 2 : 1; more == 0xFF; i++ ) {

        unsigned bank = 0, length = 0, address= 0;

        FILE *inFile = fopen ( filename, "rb" );
        if ( inFile == NULL ) {
            fprintf ( stderr, "Unable to open file \"%s\"\n", filename );
            return false;
        }

        if ( verbose > 0 ) {
            fprintf ( stdout, "Reading file \"%s\"\n", filename );
        }

        if ( type == FORMAT_FIAD ) {
            sFileDescriptorRecord fdr;
            memset ( &fdr, 0, sizeof ( fdr ));
            if ( fread ( &fdr, 128, 1, inFile ) != 1 ) {
                fprintf ( stderr, "Error reading from file \"%s\"\n", filename );
                fclose ( inFile );
                return false;
            }
            if ( fdr.FileStatus != PROGRAM_TYPE ) {
                fprintf ( stderr, "File \"%s\" is not a Gram Kracker file\n", filename );
                fclose ( inFile );
                return false;
            }
        }

        if (( fread ( &more, 1, 1, inFile ) != 1 )   ||
            ( fread ( &bank, 1, 1, inFile ) != 1 )   ||
            ( fread ( &length, 2, 1, inFile ) != 1 ) ||
            ( fread ( &address, 2, 1, inFile ) != 1 )) {
            fprintf ( stderr, "Error reading from file \"%s\"\n", filename );
            fclose ( inFile );
            return false;
        }

        length  = GetUINT16 ( &length );
        address = GetUINT16 ( &address );

        UINT8 *buffer = new UINT8 [ GROM_BANK_SIZE ];
        memset ( buffer, 0, GROM_BANK_SIZE );

        if ( fread ( buffer, 1, length, inFile ) != 1 ) {
            fprintf ( stderr, "Error reading from file \"%s\"\n", filename );
            fclose ( inFile );
            delete [] buffer;
            return false;
        }

        sMemoryRegion *region;

        if ( bank < 9 ) {
            region = &cart.GromMemory [ address >> 13 ];
            region->NumBanks = 1;
            region->Bank[0].Data = buffer;
        } else if ( bank == 9 ) {
            region = &cart.CpuMemory [ address >> 12 ];
            if ( region->NumBanks == 0 ) region->NumBanks = 1;
            region->Bank[0].Data = buffer;
            region++;
            UINT8 *newBuffer = new UINT8 [ ROM_BANK_SIZE ];
            memset ( newBuffer, 0, ROM_BANK_SIZE );
            memcpy ( newBuffer, &buffer [ length / 2 ], length / 2 );
            if ( region->NumBanks == 0 ) region->NumBanks = 1;
            region->Bank[0].Data = newBuffer;
        } else {
            region = &cart.CpuMemory [ address >> 12 ];
            region->NumBanks = 2;
            region->Bank[1].Data = buffer;
            region++;
            UINT8 *newBuffer = new UINT8 [ ROM_BANK_SIZE ];
            memset ( newBuffer, 0, ROM_BANK_SIZE );
            memcpy ( newBuffer, &buffer [ length / 2 ], length / 2 );
            region->NumBanks = 2;
            region->Bank[1].Data = newBuffer;
        }

        fclose ( inFile );

        sprintf ( filename, "%s%d%s", name, i, ext );
    }

    return true;
}

void HexDump ( FILE *file, int base, const UINT8 *pData, int length )
{
    FUNCTION_ENTRY ( NULL, "HexDump", true );

    while ( length > 0 ) {
        fprintf ( file, "%04X ", base );
        for ( int i = 0; i < 16; i++ ) {
            fprintf ( file, ( i < length ) ? "%02X " : "   ", pData [i] );
        }
        fprintf ( file, "'" );
        for ( int i = 0; i < 16; i++ ) {
            if ( i >= length ) break;
            fprintf ( file, "%c", isprint ( pData [i] ) ? pData [i] : '.' );
        }
        fprintf ( file, "' '" );
        for ( int i = 0; i < 16; i++ ) {
            if ( i >= length ) break;
            UINT8 data = ( UINT8 ) ( pData [i] + 0x60 );
            fprintf ( file, "%c", isprint ( data ) ? data : '.' );
        }
        fprintf ( file, "'\n" );
        pData  += 16;
        base   += 16;
        length -= 16;
    }
}

void DumpCartridge ( const cCartridge &cart, const char *defaultName )
{
    FUNCTION_ENTRY ( NULL, "DumpCartridge", true );

    char filename [256];
    strcpy ( filename, cart.FileName () ? cart.FileName () : defaultName );

    char *start = filename + strlen ( filename ) - 1;
    while (( start > filename ) && ( *start != FILE_SEPERATOR )) start--;
    if ( start != filename ) start++;

    char *end = start + strlen ( start ) - 1;
    while (( end > start ) && ( *end != '.' )) end--;
    if ( end != start ) *end = '\0';
    strcat ( start, ".dat" );

    FILE *file = fopen ( start, "wt" );
    if ( file == NULL ) {
        fprintf ( stderr, "Unable to open file \"%s\"\n", start );
        return;
    }

    fprintf ( file, "[%s]\n", cart.Title ());

    if ( cart.GetCRU () != 0 ) {
        fprintf ( file, "; CRU = %04X\n", cart.GetCRU ());
    }

    for ( unsigned i = 0; i < SIZE ( cart.CpuMemory ); i++ ) {
        const sMemoryRegion *ptr = &cart.CpuMemory [i];
        if ( ptr->NumBanks == 0 ) continue;
        fprintf ( file, "; ROM %d\n", i );
        for ( int j = 0; j < ptr->NumBanks; j++ ) {
            fprintf ( file, "; BANK %d - %s\n", j, types [ ptr->Bank[j].Type - 1 ]);
            if ( ptr->Bank[j].Type == BANK_ROM ) {
                HexDump ( file, i * ROM_BANK_SIZE, ptr->Bank [j].Data, ROM_BANK_SIZE );
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( cart.GromMemory ); i++ ) {
        const sMemoryRegion *ptr = &cart.GromMemory [i];
        if ( ptr->NumBanks == 0 ) continue;
        fprintf ( file, "; GROM %d\n", i );
        for ( int j = 0; j < ptr->NumBanks; j++ ) {
            fprintf ( file, "; BANK %d - %s\n", j, types [ ptr->Bank[j].Type - 1 ]);
            if ( ptr->Bank[j].Type == BANK_ROM ) {
                HexDump ( file, i * GROM_BANK_SIZE, ptr->Bank [j].Data, GROM_BANK_SIZE );
            }
        }
    }

    fclose ( file );
}

bool isProgramInformation ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "isProgramInformation", true );

    size_t len = strlen ( fileName );
    return (( len > 4 ) && ( stricmp ( &fileName [len-4], ".pif" ) == 0 )) ? true : false;
}

bool isCartridge ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "isCartridge", true );

    size_t len = strlen ( fileName );
    return (( len > 4 ) && ( stricmp ( &fileName [len-4], ".ctg" ) == 0 )) ? true : false;
}

bool isHexFile ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "isHexFile", true );

    size_t len = strlen ( fileName );
    if (( len > 4 ) && ( stricmp ( &fileName [len-4], ".hex" ) == 0 )) return true;
    if (( len > 4 ) && ( stricmp ( &fileName [len-4], ".bin" ) == 0 )) return true;

    FILE *file = fopen ( fileName, "rb" );
    if ( file != NULL ) {
        fseek ( file, 0, SEEK_END );
        size_t size = ftell ( file );
        fclose ( file );
        if ( size % 4096 == 0 ) return true;
    }

    return false;
}

bool isRomFile ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "isRomFile", true );

    if ( isHexFile ( fileName ) == false ) return false;
    size_t len = strlen ( fileName );
    if (( len >= 6 ) && ( strnicmp ( fileName + len - 6, "ti.", 3 ) == 0 )) return true;
    if (( len >= 8 ) && ( strnicmp ( fileName + len - 8, "994a.", 5 ) == 0 )) return true;
    return false;
}

bool isListing ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "isListing", true );

    size_t len = strlen ( fileName );
    if (( len > 4 ) && ( stricmp ( &fileName [len-4], ".lst" ) == 0 )) return true;
    if (( len > 4 ) && ( stricmp ( &fileName [len-4], ".dat" ) == 0 )) return true;
    return false;
}

eFileType WhichType ( const char *fileName )
{
    FUNCTION_ENTRY ( NULL, "WhichType", true );

    if ( isProgramInformation ( fileName )) return PIF;
    if ( isCartridge ( fileName )) return CTG;
    if ( isRomFile ( fileName )) return ROM;
    if ( isHexFile ( fileName )) return HEX;
    if ( isListing ( fileName )) return LST;
    return GK;
}

void PrintUsage ()
{
    FUNCTION_ENTRY ( NULL, "PrintUsage", true );

    fprintf ( stdout, "Usage: convert-ctg [options] file\n" );
    fprintf ( stdout, "\n" );
}

bool ParseCRU ( const char *arg, void *ptr )
{
    FUNCTION_ENTRY ( NULL, "ParseCRU", true );

    arg += strlen ( "cru=" );

    if ( sscanf ( arg, "%x", ( unsigned int * ) ptr ) != 1 ) {
        fprintf ( stderr, "Invalid CRU address specified: '%s'\n", arg );
        return false;
    }

    return true;
}

int main ( int argc, char *argv [] )
{
    FUNCTION_ENTRY ( NULL, "main", true );

    int baseCRU = -1;
    bool dumpCartridge = false;

    sOption optList [] = {
        {  0,  "cru*=base",  OPT_NONE,                      0,    &baseCRU,       ParseCRU, "Create a DSR cartridge at the indicated CRU address" },
        { 'd', "dump",       OPT_VALUE_SET | OPT_SIZE_BOOL, true, &dumpCartridge, NULL,     "Create a hex dump of the cartridge" },
        { 'v', "verbose*=n", OPT_VALUE_PARSE_INT,           1,    &verbose,       NULL,     "Display extra information" }
    };

    if ( argc == 1 ) {
        PrintHelp ( SIZE ( optList ), optList );
        return 0;
    }

    printf ( "Listing/.bin to TI-99/Sim .ctg file converter\n" );

    int index = 1;
    index = ParseArgs ( index, argc, argv, SIZE ( optList ), optList );

    if ( index >= argc ) {
        fprintf ( stderr, "No input file specified\n" );
        return -1;
    }

    if ( argc == 1 ) {
        fprintf ( stderr, "\nSyntax: convert <input filename> [\"Cartridge Name\"]\n" );
        return -1;
    }

    cCartridge cartridge ( NULL );

    const char *srcFilename = argv [ index++ ];
    const char *dstFilename = NULL;

    switch ( WhichType ( srcFilename )) {
        case PIF :
            ReadProgramInformation ( srcFilename, cartridge );
            break;
        case CTG :
            dstFilename = srcFilename;
            if ((( srcFilename = LocateFile ( dstFilename, "cartridges" )) == NULL ) &&
                (( srcFilename = LocateFile ( dstFilename, "roms" )) == NULL )) {
                fprintf ( stderr, "Unable to load cartridge \"%s\"\n", dstFilename );
                return -1;
            }
            if ( cartridge.LoadImage ( srcFilename ) == false ) {
                fprintf ( stderr, "The file \"%s\" does not appear to be a proper ROM cartridge\n", dstFilename );
                return -1;
            }
            break;
        case ROM :
            ReadConsole ( srcFilename, cartridge );
            return 0;
        case HEX :
            ReadHex ( srcFilename, cartridge, ( baseCRU > 0 ) ? true : false );
            break;
        case LST :
            ReadFile ( srcFilename, cartridge );
            break;
        case GK :
            ReadGK ( srcFilename, cartridge );
            break;
        default :
            fprintf ( stderr, "Unable to determine the file format for \"%s\"\n", srcFilename );
            return -1;
    }

    if ( baseCRU != -1 ) {
        cartridge.SetCRU ( baseCRU );
    }

    if ( cartridge.Title () == NULL ) {
        if ( argv [index] != NULL ) {
            cartridge.SetTitle ( argv [index] );
        } else {
            FindName ( cartridge );
        }
    }

    ShowSummary ( cartridge );

    if ( dumpCartridge == true ) {
        DumpCartridge ( cartridge, ( dstFilename == NULL ) ? srcFilename : dstFilename );
    } else {
        char filename [256];
        strcpy ( filename, ( dstFilename == NULL ) ? srcFilename : dstFilename );

        char *start = filename + strlen ( filename ) - 1;
        while (( start > filename ) && ( *start != FILE_SEPERATOR )) start--;
        if ( start != filename ) start++;

        char *end = start + strlen ( start ) - 1;
        while (( end > start ) && ( *end != '.' )) end--;
        if ( end != start ) *end = '\0';
        strcat ( start, ".ctg" );

        cartridge.SaveImage ( start );
    }

    return 0;
}
