//----------------------------------------------------------------------------
//
// File:        cartridge.cpp
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
#include "logger.hpp"
#include "compress.hpp"
#include "cartridge.hpp"
#include "support.hpp"
#include "decodelzw.hpp"
#include "encodelzw.hpp"

#if defined ( __GNUC__ )
    #include <unistd.h>
#endif

DBG_REGISTER ( __FILE__ );

enum eMemoryRegion {
    ROM_0,		// 0x0000 - 0x0FFF
    ROM_1,		// 0x1000 - 0x1FFF
    ROM_2,		// 0x2000 - 0x2FFF
    ROM_3,		// 0x3000 - 0x3FFF
    ROM_4,		// 0x4000 - 0x4FFF
    ROM_5,		// 0x5000 - 0x5FFF
    ROM_6,		// 0x6000 - 0x6FFF
    ROM_7,		// 0x7000 - 0x7FFF
    ROM_8,		// 0x8000 - 0x8FFF
    ROM_9,		// 0x9000 - 0x9FFF
    ROM_A,		// 0xA000 - 0xAFFF
    ROM_B,		// 0xB000 - 0xBFFF
    ROM_C,		// 0xC000 - 0xCFFF
    ROM_D,		// 0xD000 - 0xDFFF
    ROM_E,		// 0xE000 - 0xEFFF
    ROM_F,		// 0xF000 - 0xFFFF
    GROM_0,		// 0x0000 - 0x1FFF
    GROM_1,		// 0x2000 - 0x3FFF
    GROM_2,		// 0x4000 - 0x5FFF
    GROM_3,		// 0x6000 - 0x7FFF
    GROM_4,		// 0x8000 - 0x9FFF
    GROM_5,		// 0xA000 - 0xBFFF
    GROM_6,		// 0xC000 - 0xDFFF
    GROM_7 		// 0xE000 - 0xFFFF
};

const int FILE_VERSION = 0x20;

const char *cCartridge::sm_Banner = "TI-99/4A Module - ";

cCartridge::cCartridge ( const char *filename ) :
    m_FileName ( NULL ),
    m_RamFileName ( NULL ),
    m_Title ( NULL ),
    m_BaseCRU ( 0 )
{
    FUNCTION_ENTRY ( this, "cCartridge ctor", true );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        CpuMemory[i].NumBanks = 0;
        CpuMemory[i].CurBank = NULL;
        for ( unsigned j = 0; j < 4; j++ ) {
            CpuMemory[i].Bank[j].Type = BANK_ROM;
            CpuMemory[i].Bank[j].Data = NULL;
        }
    }
    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        GromMemory[i].NumBanks = 0;
        GromMemory[i].CurBank = NULL;
        for ( unsigned j = 0; j < 4; j++ ) {
            GromMemory[i].Bank[j].Type = BANK_ROM;
            GromMemory[i].Bank[j].Data = NULL;
        }
    }

    if ( LoadImage ( filename ) == false ) {
        DBG_ERROR ( "Cartridge " << filename << " is invalid" );
    }
}

cCartridge::~cCartridge ()
{
    FUNCTION_ENTRY ( this, "cCartridge dtor", true );

    if ( SaveRAM () == false ) {
        DBG_WARNING ( "Non-volatile memory lost" );
    }

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory[i].NumBanks; j++ ) {
            delete [] CpuMemory[i].Bank[j].Data;
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory[i].NumBanks; j++ ) {
            delete [] GromMemory[i].Bank[j].Data;
        }
    }

    memset ( CpuMemory, 0, sizeof ( CpuMemory ));
    memset ( GromMemory, 0, sizeof ( GromMemory ));

    delete [] m_Title;
    delete [] m_RamFileName;
    delete [] m_FileName;
}

void cCartridge::SetFileName ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::SetFileName", true );

    delete [] m_FileName;
    delete [] m_RamFileName;

    if ( filename == NULL ) {
        m_FileName    = NULL;
        m_RamFileName = NULL;
        return;
    }

    size_t length = strlen ( filename );
    m_FileName = new char [ length + 1 ];
    strcpy ( m_FileName, filename );

    const char *ptr = m_FileName + length - 1;
    while ( ptr > m_FileName ) {
        if ( *ptr == FILE_SEPERATOR ) {
            ptr++;
            break;
        }
        ptr--;
    }
    length = strlen ( ptr );

    m_RamFileName = new char [ strlen ( HOME_PATH ) + 1 + length + 1 ];
    sprintf ( m_RamFileName, "%s%c%*.*s.ram", HOME_PATH, FILE_SEPERATOR, ( int ) ( length - 4 ), ( int ) ( length - 4 ), ptr );
}

void cCartridge::SetTitle ( const char *title )
{
    FUNCTION_ENTRY ( this, "cCartridge::SetTitle", true );

    delete [] m_Title;

    if ( title == NULL ) title = "<Unknown>";
    m_Title = new char [ strlen ( title ) + 1 ];
    strcpy ( m_Title, title );
}

bool cCartridge::IsValid () const
{
    FUNCTION_ENTRY ( this, "cCartridge::IsValid", true );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        if ( CpuMemory[i].NumBanks != 0 ) return true;
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        if ( GromMemory[i].NumBanks != 0 ) return true;
    }

    return false;
}

bool cCartridge::LoadRAM () const
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadRAM", true );

    FILE *file = m_RamFileName ? fopen ( m_RamFileName, "rb" ) : NULL;
    if ( file == NULL ) return true;

    DBG_EVENT ( "Loading module RAM: " << Title ());

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            // If this bank is RAM & Battery backed - update the cartridge
            if ( CpuMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                if ( LoadBuffer ( ROM_BANK_SIZE, CpuMemory [i].Bank [j].Data, file ) == false ) {
                    DBG_ERROR ( "Unable to load RAM data" );
                    return false;
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            // If this bank is RAM & Battery backed - update the cartridge
            if ( CpuMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                if ( LoadBuffer ( GROM_BANK_SIZE, GromMemory [i].Bank [j].Data, file ) == false ) {
                    DBG_ERROR ( "Unable to load GROM data" );
                    return false;
                }
            }
        }
    }

    fclose ( file );

    return true;
}

bool cCartridge::SaveRAM () const
{
    FUNCTION_ENTRY ( this, "cCartridge::SaveRAM", true );

    // Don't bother creating a .ram file if there is nothing stored in the RAM

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            if ( CpuMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                for ( unsigned k = 0; k < ROM_BANK_SIZE; k++ ) {
                    if ( CpuMemory[i].Bank[j].Data[k] != 0 ) goto save;
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            if ( GromMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                for ( unsigned k = 0; k < GROM_BANK_SIZE; k++ ) {
                    if ( GromMemory[i].Bank[j].Data[k] != 0 ) goto save;
                }
            }
        }
    }

    if ( m_RamFileName != NULL ) {
        unlink ( m_RamFileName );
    }

    return true;

save:

    FILE *file = m_RamFileName ? fopen ( m_RamFileName, "wb" ) : NULL;
    if ( file == NULL ) {
        DBG_ERROR ( "Unable to open RAM file" );
        return false;
    }

    DBG_EVENT ( "Saving module RAM: " << Title ());

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        for ( int j = 0; j < CpuMemory [i].NumBanks; j++ ) {
            // If this bank is battery-backed RAM then update the .ram file
            if ( CpuMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                if ( SaveBuffer ( ROM_BANK_SIZE, CpuMemory[i].Bank[j].Data, file ) == false ) {
                    DBG_ERROR ( "Error writing ROM data" );
                    return false;
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        for ( int j = 0; j < GromMemory [i].NumBanks; j++ ) {
            // If this bank is battery-backed RAM then update the .ram file
            if ( GromMemory[i].Bank[j].Type == BANK_BATTERY_BACKED ) {
                if ( SaveBuffer ( GROM_BANK_SIZE, GromMemory[i].Bank[j].Data, file ) == false ) {
                    DBG_ERROR ( "Error writing GROM data" );
                    return false;
                }
            }
        }
    }

    fclose ( file );

    return true;
}

bool cCartridge::EncodeCallback ( void *, size_t size, void *ptr )
{
    FUNCTION_ENTRY ( NULL, "cCartridge::EncodeCallback", false );

    * ( size_t * ) ptr = size;

    return true;
}

bool cCartridge::DecodeCallback ( void *, size_t, void * )
{
    FUNCTION_ENTRY ( NULL, "cCartridge::DecodeCallback", false );

    return true;
}

bool cCartridge::SaveBufferLZW ( void *buffer, size_t size, FILE *file )
{
    FUNCTION_ENTRY ( NULL, "cCartridge::SaveBufferLZW", false );

    DBG_ASSERT ( size < 0x8000 );

    if ( buffer == NULL ) {
        DBG_ERROR ( "Internal error - buffer is NULL" );
        return false;
    }

    bool retVal = false;

    size_t outSize   = 0;
    UINT8 *outBuffer = new UINT8 [ 2 * size ];

    cEncodeLZW encoder ( 15 );

    encoder.SetWriteCallback ( EncodeCallback, outBuffer, 2 * size, &outSize );

    if ( encoder.EncodeBuffer ( buffer, size ) != 1 ) {
        DBG_ERROR ( "Error compressing data" );
    } else {
        void *ptr = outBuffer;
        // Make sure we didn't make things worse
        if ( outSize > size ) {
            outSize = 0x8000 | size;
            ptr = buffer;
        }
        fputc (( int ) ( outSize >> 8 ), file );
        fputc (( int ) ( outSize & 0xFF ), file );
        if ( fwrite ( ptr, 1, outSize & 0x7FFF, file ) != ( outSize & 0x7FFF )) {
            DBG_ERROR ( "Error writing to file" );
        } else {
            retVal = true;
        }
    }

    delete [] outBuffer;

    return retVal;
}

bool cCartridge::LoadBufferLZW ( void *buffer, size_t size, FILE *file )
{
    FUNCTION_ENTRY ( NULL, "cCartridge::LoadBufferLZW", false );

    size_t inSize = 0;
    inSize = ( UINT8 ) fgetc ( file ) << 8;
    inSize = inSize | ( UINT8 ) fgetc ( file );

    bool retVal = false;

    if ( inSize & 0x8000 ) {

        if ( fread ( buffer, 1, inSize & 0x7FFF, file ) != ( inSize & 0x7FFF )) {
            DBG_ERROR ( "Error reading from file" );
        } else {
            retVal = true;
        }

    } else {

        cDecodeLZW decoder ( 15 );

        decoder.SetWriteCallback ( DecodeCallback, buffer, size, NULL );

        UINT8 *inBuffer = new UINT8 [ inSize ];

        if ( fread ( inBuffer, 1, inSize, file ) != inSize ) {
            DBG_ERROR ( "Error reading from file" );
        } else {
            if ( decoder.ParseBuffer ( inBuffer, inSize ) != 1 ) {
                DBG_ERROR ( "Invalid LZW data" );
            } else {
                retVal = true;
            }
        }

        delete [] inBuffer;
    }

    return retVal;
}

bool cCartridge::LoadBufferRLE ( void *, size_t, FILE * )
{
    FUNCTION_ENTRY ( NULL, "cCartridge::LoadBufferRLE", false );

    return true;
}

//----------------------------------------------------------------------------
//
// Version 0:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//      0000   1  tag
//     [0001   2  CRU base] only if marked as DSR
//      0001   1  type
//      0002   1  # banks
//      0003   4  bank sizes
//      [0007 ##  RLE compressed data] (only if type == ROM)
//
//
// Version 1:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//     0050    1  Version marker (0x10)
//     0051    2  CRU base
//      0000   1  index
//      0001   1  # banks
//       0000  1  bank 1 type
//      [0001 ##  RLE compressed data] (only if type == ROM)
//
//
// Version 2:
//
//   Offset Size  Contents
//     0000   80  Banner - "TI-99/4A Module - <NAME>"
//     0050    1  Version marker (0x20)
//
// Manufacturer
// Copyright/date
// Catalog Number
// Icon/Image
//
//     0051    2  CRU base
//      0000   1  index
//      0001   1  # banks
//       0000  1  bank 1 type
//      [0000  2  Size of data] (only if type == ROM)
//      [0002 ##  LZW compressed data] (only if type == ROM)
//
//----------------------------------------------------------------------------

bool cCartridge::LoadImageV0 ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadImageV0", true );

    UINT8 tag = ( UINT8 ) fgetc ( file );

    while ( ! feof ( file )) {

        bool dsr = ( tag & 0x40 ) ? true : false;
        UINT8 index = ( UINT8 ) ( tag & 0x3F );

        sMemoryRegion *memory = NULL;
        UINT16 size = 0;

        if ( index < GROM_0 ) {
            memory = &CpuMemory [ index ];
            size   = ROM_BANK_SIZE;
        } else {
            index -= GROM_0;
            memory = &GromMemory [ index ];
            size   = GROM_BANK_SIZE;
        }

        if ( dsr ) {
            m_BaseCRU = ( UINT16 ) fgetc ( file );
            m_BaseCRU = ( UINT16 ) ( m_BaseCRU | (( UINT8 ) fgetc ( file ) << 8 ));
            DBG_EVENT ( "  CRU Base: " << hex << m_BaseCRU );
        }

        BANK_TYPE_E type = ( BANK_TYPE_E ) ( fgetc ( file ) + 1 );

        memory->NumBanks = ( UINT16 ) fgetc ( file );
        UINT16 NumBytes [4];
        if ( fread ( NumBytes, 1, sizeof ( NumBytes ), file ) != sizeof ( NumBytes )) {
            DBG_ERROR ( "Error reading from file" );
            return false;
        }

        DBG_EVENT ( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

        for ( int i = 0; i < memory->NumBanks; i++ ) {
            memory->Bank[i].Type = type;
            memory->Bank[i].Data = new UINT8 [ size ];
            memset ( memory->Bank[i].Data, 0, size );
            if ( type == BANK_ROM ) {
                if ( LoadBuffer ( NumBytes [i], memory->Bank[i].Data, file ) == false ) {
                    DBG_ERROR ( "Error reading data from file" );
                    return false;
                }
            }
        }
        memory->CurBank = &memory->Bank[0];

        tag = ( UINT8 ) fgetc ( file );
    }

    return true;
}

bool cCartridge::LoadImageV1 ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadImageV1", true );

    m_BaseCRU = ( UINT16 ) (( UINT8 ) fgetc ( file ) << 8 );
    m_BaseCRU = ( UINT16 ) ( m_BaseCRU | ( UINT8 ) fgetc ( file ));

    UINT8 index = ( UINT8 ) fgetc ( file );

    while ( ! feof ( file )) {

        sMemoryRegion *memory = NULL;
        UINT16 size = 0;

        if ( index < GROM_0 ) {
            memory = &CpuMemory [ index ];
            size   = ROM_BANK_SIZE;
        } else {
            index -= GROM_0;
            memory = &GromMemory [ index ];
            size   = GROM_BANK_SIZE;
        }

        memory->NumBanks = ( int ) fgetc ( file );

        DBG_EVENT ( "  " << (( size != GROM_BANK_SIZE ) ? " RAM" : "GROM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

        for ( int i = 0; i < memory->NumBanks; i++ ) {
            memory->Bank[i].Type = ( BANK_TYPE_E ) fgetc ( file );
            memory->Bank[i].Data = new UINT8 [ size ];
            memset ( memory->Bank[i].Data, 0, size );
            if ( memory->Bank[i].Type == BANK_ROM ) {
                if ( LoadBuffer ( size, memory->Bank[i].Data, file ) == false ) {
                    DBG_ERROR ( "Error reading data from file" );
                    return false;
                }
            }
        }
        memory->CurBank = &memory->Bank[0];

        index = ( UINT8 ) fgetc ( file );
    }

    return true;
}

bool cCartridge::LoadImageV2 ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadImageV2", true );

    m_BaseCRU = ( UINT16 ) (( UINT8 ) fgetc ( file ) << 8 );
    m_BaseCRU = ( UINT16 ) ( m_BaseCRU | ( UINT8 ) fgetc ( file ));

    UINT8 index = ( UINT8 ) fgetc ( file );

    while ( ! feof ( file )) {

        sMemoryRegion *memory = NULL;
        size_t size = 0;

        if ( index < GROM_0 ) {
            memory = &CpuMemory [ index ];
            size   = ROM_BANK_SIZE;
        } else {
            index -= GROM_0;
            memory = &GromMemory [ index ];
            size   = GROM_BANK_SIZE;
        }

        memory->NumBanks = ( int ) fgetc ( file );

        for ( int i = 0; i < memory->NumBanks; i++ ) {
            memory->Bank[i].Type = ( BANK_TYPE_E ) fgetc ( file );
            memory->Bank[i].Data = new UINT8 [ size ];
            memset ( memory->Bank[i].Data, 0, size );
            if ( memory->Bank[i].Type == BANK_ROM ) {
                if ( LoadBufferLZW ( memory->Bank[i].Data, size, file ) == false ) {
                    return false;
                }
            }
        }

        memory->CurBank = &memory->Bank[0];

        DBG_EVENT ( "  " << (( size != GROM_BANK_SIZE ) ?
            memory->CurBank->Type == BANK_ROM ? "ROM" : "RAM" :
            memory->CurBank->Type == BANK_ROM ? "GROM" : "GRAM" ) << " @ " << hex << ( UINT16 ) ( index * size ));

        index = ( UINT8 ) fgetc ( file );
    }

    return true;
}

bool cCartridge::LoadImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::LoadImage", true );

    DBG_TRACE ( "Opening file " << filename );

    FILE *file = filename ? fopen ( filename, "rb" ) : NULL;
    if ( file == NULL ) {
        DBG_WARNING ( "Unable to locate file " << filename );
        return false;
    }

    SetFileName ( filename );

    // Make sure this is really a TI-99/4A cartridge file
    char buffer [ 80 ];
    if ( fread ( buffer, 1, sizeof ( buffer ), file ) != sizeof ( buffer )) {
        DBG_ERROR ( "Error reading from file" );
        return false;
    }

    bool retVal = false;

    if ( strncmp ( buffer, sm_Banner, strlen ( sm_Banner )) != 0 ) {

        DBG_WARNING ( "Invalid file format" );

    } else {

        char *ptr = &buffer [ strlen ( sm_Banner )];
        ptr [ strlen ( ptr ) - 2 ] = '\0';
        SetTitle ( ptr );

        DBG_EVENT ( "Loading module: " << Title ());

        int version = ( UINT8 ) fgetc ( file );
        if (( version & 0x80 ) != 0 ) {
            if ( ungetc ( version, file ) != version ) {
                DBG_ERROR ( "Unable to unget character to file" );
            } else {
                retVal = LoadImageV0 ( file );
            }
        } else {
            switch ( version & 0xF0 ) {
                case 0x10 :
                    retVal = LoadImageV1 ( file );
                    break;
                case 0x20 :
                    retVal = LoadImageV2 ( file );
                    break;
                default :
                    DBG_ERROR ( "Unrecognized file version" );
                    break;
            }
        }

        if ( retVal == true ) {
            retVal = LoadRAM ();
        }
    }

    fclose ( file );

    return retVal;
}

bool cCartridge::SaveImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cCartridge::SaveImage", true );

    FILE *file = filename ? fopen ( filename, "wb" ) : NULL;
    if ( file == NULL ) return false;

    SetFileName ( filename );

    char buffer [ 80 ];
    memset ( buffer, 0, sizeof ( buffer ));
    sprintf ( buffer, "%s%s\n%c", sm_Banner, Title (), 0x1A );
    fwrite ( buffer, sizeof ( buffer ), 1, file );

    DBG_EVENT ( "Saving module: " << Title ());

    fputc ( FILE_VERSION, file );

    fputc ( m_BaseCRU >> 8, file );
    fputc ( m_BaseCRU & 0xFF, file );

    for ( unsigned i = 0; i < SIZE ( CpuMemory ); i++ ) {
        if ( CpuMemory[i].NumBanks != 0 ) {
            sMemoryRegion *memory = &CpuMemory[i];
            fputc (( UINT8 ) ( ROM_0 + i ), file );
            fputc ( memory->NumBanks, file );
            for ( int j = 0; j < memory->NumBanks; j++ ) {
                fputc ( memory->Bank[j].Type, file );
                if ( memory->Bank[j].Type == BANK_ROM ) {
                    if ( SaveBufferLZW ( memory->Bank[j].Data, ROM_BANK_SIZE, file ) == false ) {
                        fclose ( file );
                        return false;
                    }
                }
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( GromMemory ); i++ ) {
        if ( GromMemory[i].NumBanks != 0 ) {
            sMemoryRegion *memory = &GromMemory[i];
            fputc (( UINT8 ) ( GROM_0 + i ), file );
            fputc ( memory->NumBanks, file );
            for ( int j = 0; j < memory->NumBanks; j++ ) {
                fputc ( memory->Bank[j].Type, file );
                if ( memory->Bank[j].Type == BANK_ROM ) {
                    if ( SaveBufferLZW ( memory->Bank[j].Data, GROM_BANK_SIZE, file ) == false ) {
                        fclose ( file );
                        return false;
                    }
                }
            }
        }
    }

    fclose ( file );

    return true;
}

void cCartridge::DumpRegion ( FILE *file, const char *label, const sMemoryRegion *region, unsigned count, int bankSize, BANK_TYPE_E type, bool forceDisplay ) const
{
    FUNCTION_ENTRY ( NULL, "cCartridge::DumpRegion", true );

    bool foundType = false;
    for ( unsigned i = 0; i < count; i++ ) {
        if (( region[i].NumBanks > 0 ) && ( region[i].Bank[0].Type == type )) {
            foundType = true;
            break;
        }
    }

    if ( foundType == false ) {
        if ( forceDisplay == true ) {
            fprintf ( file, "%6.6s: -NONE-\n", label );
        }
        return;
    }

    fprintf ( file, "%6.6s:", label );

    for ( unsigned i = 0; i < count; i++ ) {
        if (( region[i].NumBanks > 0 ) && ( region[i].Bank[0].Type == type )) {
            fprintf ( file, " %04X", i * bankSize );
            if ( region[i].NumBanks > 1 ) {
                fprintf ( file, "(%d)", region[i].NumBanks );
            }
        }
    }

    fprintf ( file, "\n" );
}

void cCartridge::PrintInfo ( FILE *file ) const
{
    FUNCTION_ENTRY ( NULL, "cCartridge::PrintInfo", true );

    fprintf ( file, "\n" );

    fprintf ( file, "  File: \"%s\"\n", FileName ());
    fprintf ( file, " Title: \"%s\"\n", Title ());

    fprintf ( file, "\n" );

    DumpRegion ( file, "ROM", CpuMemory, SIZE ( CpuMemory ), ROM_BANK_SIZE, BANK_ROM, true );
    DumpRegion ( file, "RAM", CpuMemory, SIZE ( CpuMemory ), ROM_BANK_SIZE, BANK_RAM, false );
    DumpRegion ( file, "GROM", GromMemory, SIZE ( GromMemory ), GROM_BANK_SIZE, BANK_ROM, true );
    DumpRegion ( file, "GRAM", GromMemory, SIZE ( GromMemory ), GROM_BANK_SIZE, BANK_RAM, false );

    fprintf ( file, "\n" );
}
