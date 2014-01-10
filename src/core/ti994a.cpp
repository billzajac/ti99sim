//----------------------------------------------------------------------------
//
// File:        ti994a.cpp
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

#include <exception>
#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "compress.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "tms9919.hpp"
#include "tms5220.hpp"
#include "cartridge.hpp"
#include "ti994a.hpp"
#include "device.hpp"
#include "tms9901.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

extern "C" UINT8 CpuMemory [ 0x10000 ];

extern "C" {

    void *CRU_Object;

}

extern "C" int (*TimerHook) ();

static cTI994A *pTimerObj;

cTI994A::cTI994A ( cCartridge *_console, cTMS9918A *_vdp, cTMS9919 *_sound, cTMS5220 *_speech ) :
    m_CPU ( NULL ),
    m_PIC ( NULL ),
    m_VDP ( NULL ),
    m_SoundGenerator ( NULL ),
    m_SpeechSynthesizer ( _speech ),
    m_RetraceInterval ( 0 ),
    m_LastRetrace ( 0 ),
    m_Console ( NULL ),
    m_Cartridge ( NULL ),
    m_ActiveCRU ( 0 ),
    m_GromPtr ( NULL ),
    m_GromAddress ( 0 ),
    m_GromLastInstruction ( 0 ),
    m_GromReadShift ( 8 ),
    m_GromWriteShift ( 8 ),
    m_GromCounter ( 0 ),
    m_CpuMemory ( CpuMemory ),
    m_GromMemory ( NULL ),
    m_VideoMemory ( NULL )
{
    FUNCTION_ENTRY ( this, "cTI994A ctor", true );

    // Store a copy of our 'this' pointer for the TimerHook function
    pTimerObj = this;

    // Define CRU_Object for _OpCodes.asm
    CRU_Object = this;

    m_GromMemory = new UINT8 [ 0x10000 ];

    memset ( CpuMemory, 0, 0x10000 );
    memset ( m_GromMemory, 0, 0x10000 );

    memset ( m_CpuMemoryInfo, 0, sizeof ( m_CpuMemoryInfo ));
    memset ( m_GromMemoryInfo, 0, sizeof ( m_GromMemoryInfo ));

    m_CPU = new cTMS9900 ();
    m_PIC = new cTMS9901 ( m_CPU );

    m_VDP = ( _vdp != NULL ) ? _vdp : new cTMS9918A ();
    m_VDP->SetPIC ( m_PIC, 2 );

    m_VideoMemory = m_VDP->GetMemory ();

    m_SoundGenerator = ( _sound != NULL ) ? _sound : new cTMS9919;

    if ( m_SpeechSynthesizer != NULL ) {
        m_SpeechSynthesizer->SetComputer ( this );
    }

    // Start off with all memory set as ROM - simulates no memory present
    m_CPU->SetMemory ( MEM_ROM, 0x0000, 0x10000 );

    // No Devices by default - derived classes can add Devices
    m_ActiveCRU = 0;
    memset ( m_Device, 0, sizeof ( m_Device ));

    // Add the TMS9901 programmable timer
    m_Device [0] = m_PIC;

    InsertCartridge ( _console );

    m_Cartridge = NULL;
    m_Console   = _console;

    TimerHook = _TimerHookProc;

    m_RetraceInterval = CPU_SPEED_HZ / m_VDP->GetRefreshRate ();

    m_GromPtr = m_GromMemory;

    UINT8 index;
    // Register the bank swap trap function here - not used until needed
    m_CPU->RegisterTrapHandler ( TrapFunction, this, TRAP_BANK_SWITCH );

    // Mark the scratchpad RAM area so that we alias it correctly
    m_CPU->SetMemory ( MEM_PAD, 0x8000, 0x0300 );

    index = m_CPU->RegisterTrapHandler ( TrapFunction, this, TRAP_SOUND );
    for ( UINT16 address = 0x8400; address < 0x8800; address += ( UINT16 ) 1 ) {
        m_CPU->SetTrap ( address, MEMFLG_TRAP_WRITE, index );					// Sound chip Port
    }

    index = m_CPU->RegisterTrapHandler ( TrapFunction, this, TRAP_VIDEO );
    for ( UINT16 address = 0x8800; address < 0x8C00; address += ( UINT16 ) 4 ) {
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0000 ), MEMFLG_TRAP_READ, index );		// VDP Read Byte Port
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0002 ), MEMFLG_TRAP_READ, index );		// VDP Read Status Port
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0400 ), MEMFLG_TRAP_WRITE, index );		// VDP Write Byte Port
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0402 ), MEMFLG_TRAP_WRITE, index );		// VDP Write (set) Address Port
    }

    // Make this bank look like ROM (writes aren't stored)
    m_CPU->SetMemory ( MEM_ROM, 0x9000, ROM_BANK_SIZE );

    index = m_CPU->RegisterTrapHandler ( TrapFunction, this, TRAP_SPEECH );
    for ( UINT16 address = 0x9000; address < 0x9400; address += ( UINT16 ) 2 ) {
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0000 ), ( UINT8 ) MEMFLG_TRAP_READ, index );	// Speech Read Port
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0400 ), ( UINT8 ) MEMFLG_TRAP_WRITE, index );	// Speech Write Port
    }

    index = m_CPU->RegisterTrapHandler ( TrapFunction, this, TRAP_GROM );
    for ( UINT16 address = 0x9800; address < 0x9C00; address += ( UINT16 ) 2 ) {
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0000 ), ( UINT8 ) MEMFLG_TRAP_READ, index );	// GROM Read Port
        m_CPU->SetTrap (( UINT16 ) ( address | 0x0400 ), ( UINT8 ) MEMFLG_TRAP_WRITE, index );	// GROM Write Port
    }
}

cTI994A::~cTI994A ()
{
    FUNCTION_ENTRY ( this, "cTI994A dtor", true );

    for ( unsigned i = 0; i < SIZE ( m_Device ); i++ ) {
        if ( m_Device [i] != NULL ) {
            delete m_Device [i];
        }
    }

    delete [] m_GromMemory;

    delete m_SpeechSynthesizer;
    delete m_SoundGenerator;
    delete m_CPU;
    delete m_VDP;
    delete m_Console;
}

cDevice *cTI994A::GetDevice ( ADDRESS address ) const
{
    FUNCTION_ENTRY ( this, "cTI994A::GetDevice", false );

    // CRU Allocations:
    //  0000-0FFE       System Console
    //  1000-10FE       Unassigned
    //  1100-11FE       Disk Controller Card
    //  1200-12FE       Reserved
    //  1300-13FE       RS-232 (primary)
    //  1400-14FE       Unassigned
    //  1500-15FE       RS-232 (secondary)
    //  1600-16FE       Unassigned
    //  1700-17FE       HEX-BUS™ Interface
    //  1800-18FE       Thermal Printer
    //  1900-10FE       Reserved
    //  1A00-1AFE       Unassigned
    //  1B00-1BFE       Unassigned
    //  1C00-1CFE       Video Controler Card
    //  1D00-1DFE       IEE-488 Bus Controler Card
    //  1E00-1EFE       Unassigned
    //  1F00-1FFE       P-Code Card

    return m_Device [ ( address >> 8 ) & 0x1F ];
}

int cTI994A::_TimerHookProc ()
{
    FUNCTION_ENTRY ( NULL, "cTI994A::_TimerHookProc", false );

    return pTimerObj->TimerHookProc ();
}

int cTI994A::TimerHookProc ()
{
    FUNCTION_ENTRY ( this, "cTI994A::TimerHookProc", false );

    UINT32 clockCycles = m_CPU->GetClocks ();

    // Simulate a 50/60Hz VDP interrupt
    if ( clockCycles - m_LastRetrace > m_RetraceInterval ) {
        m_LastRetrace += m_RetraceInterval;
        m_VDP->Retrace ();
    }

    return 0;
}

UINT8 cTI994A::TrapFunction ( void *ptr, int type, bool read, ADDRESS address, UINT8 value )
{
    FUNCTION_ENTRY ( ptr, "cTI994A::TrapFunction", false );

    cTI994A *pThis = ( cTI994A * ) ptr;
    UINT8 retVal = value;

    if ( read == true ) {
        switch ( type ) {
            case TRAP_SOUND :
                retVal = pThis->SoundBreakPoint ( address, value );
                break;
            case TRAP_SPEECH :
                retVal = pThis->SpeechReadBreakPoint ( address, value );
                break;
            case TRAP_VIDEO :
                retVal = pThis->VideoReadBreakPoint (( UINT16 ) ( address & 0xFC02 ), value );
                break;
            case TRAP_GROM :
                retVal = pThis->GromReadBreakPoint (( UINT16 ) ( address & 0xFC02 ), value );
                break;
            default :
                fprintf ( stderr, "Invalid index %d for read access in TrapFunction\n", type );
                break;
        };
    } else {
        switch ( type ) {
            case TRAP_BANK_SWITCH :
                retVal = pThis->BankSwitch ( address, value );
                break;
            case TRAP_SOUND :
                retVal = pThis->SoundBreakPoint ( address, value );
                break;
            case TRAP_SPEECH :
                retVal = pThis->SpeechWriteBreakPoint ( address, value );
                break;
            case TRAP_VIDEO :
                retVal = pThis->VideoWriteBreakPoint (( UINT16 ) ( address & 0xFC02 ), value );
                break;
            case TRAP_GROM :
                retVal = pThis->GromWriteBreakPoint (( UINT16 ) ( address & 0xFC02 ), value );
                break;
            default :
                fprintf ( stderr, "Invalid index %d for write access in TrapFunction\n", type );
                break;
        };
    }

    return retVal;
}

UINT8 cTI994A::BankSwitch ( ADDRESS address, UINT8 )
{
    FUNCTION_ENTRY ( this, "cTI994A::BankSwitch", false );

    sMemoryRegion *region = m_CpuMemoryInfo [( address >> 12 ) & 0xFE ];
    int newBank = ( address >> 1 ) % region->NumBanks;
    region->CurBank = &region->Bank[newBank];
    ADDRESS baseAddress = ( ADDRESS ) ( address & 0xE000 );
    memcpy ( &CpuMemory [ baseAddress ], region->CurBank->Data, ROM_BANK_SIZE );
    region++;
    region->CurBank = &region->Bank[newBank];
    memcpy ( &CpuMemory [ baseAddress + ROM_BANK_SIZE ], region->CurBank->Data, ROM_BANK_SIZE );

    return CpuMemory [ address ];
}

UINT8 cTI994A::SoundBreakPoint ( ADDRESS, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SoundBreakPoint", false );

    m_SoundGenerator->WriteData ( data );

    return data;
}

UINT8 cTI994A::SpeechWriteBreakPoint ( ADDRESS, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SpeechWriteBreakPoint", false );

    if ( m_SpeechSynthesizer != NULL ) {
        m_SpeechSynthesizer->WriteData ( data );
    }

    return data;
}

UINT8 cTI994A::SpeechReadBreakPoint ( ADDRESS, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::SpeechReadBreakPoint", false );

    if ( m_SpeechSynthesizer != NULL ) {
        data = m_SpeechSynthesizer->ReadData ( data );
    }

    return data;
}

UINT8 cTI994A::VideoReadBreakPoint ( ADDRESS address, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::VideoReadBreakPoint", false );

    switch ( address ) {
        case 0x8800 :
            data = m_VDP->ReadData ();
            break;
        case 0x8802 :
            data = m_VDP->ReadStatus ();
            break;
        default :
            DBG_FATAL ( "Unexpected address " << hex << address );
            break;
    }
    return data;
}

UINT8 cTI994A::VideoWriteBreakPoint ( ADDRESS address, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::VideoWriteBreakPoint", false );

    switch ( address ) {
        case 0x8C00 :
            m_VDP->WriteData ( data );
            break;
        case 0x8C02 :
            m_VDP->WriteAddress ( data );
            break;
        default :
            DBG_FATAL ( "Unexpected address " << hex << address );
            break;
    }
    return data;
 }

UINT8 cTI994A::GromReadBreakPoint ( ADDRESS address, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::GromReadBreakPoint", false );

    m_GromWriteShift = 8;

    switch ( address ) {
        case 0x9800 :			// GROM/GRAM Read Byte Port
            data = *m_GromPtr;
            m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
            break;
        case 0x9802 :			// GROM/GRAM Read Address Port
            data = ( UINT8 ) ((( m_GromAddress + 1 ) >> m_GromReadShift ) & 0x00FF );
            m_GromReadShift  = 8 - m_GromReadShift;
            break;
        default :
            DBG_FATAL ( "Unexpected address " << hex << address );
            break;
    }

    m_GromPtr = &m_GromMemory [ m_GromAddress ];

    return data;
}

UINT8 cTI994A::GromWriteBreakPoint ( ADDRESS address, UINT8 data )
{
    FUNCTION_ENTRY ( this, "cTI994A::GromWriteBreakPoint", false );

    sMemoryRegion *memory;
    switch ( address ) {
        case 0x9C00 :			// GROM/GRAM Write Byte Port
            memory = m_GromMemoryInfo [ m_GromAddress >> 13 ];
            if ( memory && ( memory->CurBank->Type != BANK_ROM )) *m_GromPtr = data;
            m_GromAddress = ( UINT16 ) (( m_GromAddress & 0xE000 ) | (( m_GromAddress + 1 ) & 0x1FFF ));
            m_GromWriteShift = 8;
            break;
        case 0x9C02 :			// GROM/GRAM Write (set) Address Port
            m_GromAddress &= ( ADDRESS ) ( 0xFF00 >> m_GromWriteShift );
            m_GromAddress |= ( ADDRESS ) ( data << m_GromWriteShift );
            m_GromWriteShift = 8 - m_GromWriteShift;
            m_GromReadShift  = 8;
            break;
        default :
            DBG_FATAL ( "Unexpected address " << hex << address );
            break;
    }

    m_GromPtr = &m_GromMemory [ m_GromAddress ];

    return data;
}

extern "C" void WriteCRU ( cTI994A *ti, ADDRESS address, int count, UINT16 value )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::WriteCRU", false );

    while ( count-- ) {
        ti->WriteCRU (( ADDRESS ) ( address++ & 0x1FFF ), ( UINT16 ) ( value & 1 ));
        value >>= 1;
    }
}

extern "C" int ReadCRU ( cTI994A *ti, ADDRESS address, int count )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::ReadCRU", false );

    int value = 0;
    address += ( UINT16 ) count;
    while ( count-- ) {
        value <<= 1;
        value |= ti->ReadCRU (( ADDRESS ) ( --address & 0x1FFF ));
    }
    return value;
}

int cTI994A::ReadCRU ( ADDRESS address )
{
    FUNCTION_ENTRY ( this, "cTI994A::ReadCRU", false );

    address <<= 1;
    cDevice *dev = GetDevice ( address );
    return ( dev != NULL ) ? dev->ReadCRU (( ADDRESS ) (( address - dev->GetCRU ()) >> 1 )) : 1;
}

void cTI994A::WriteCRU ( ADDRESS address, UINT16 val )
{
    FUNCTION_ENTRY ( this, "cTI994A::WriteCRU", false );

    address <<= 1;
    cDevice *dev = GetDevice ( address );

    if ( dev == NULL ) return;

    // See if we need to swap in/out the DSR ROM routines
    if (( address != 0 ) && ( dev->GetCRU () == address )) {
        cCartridge *ctg = m_Cartridge;
        if ( val == 1 ) {
            m_ActiveCRU = address;
            m_Cartridge = NULL;
            InsertCartridge ( dev->GetROM (), false );
            dev->Activate ();
        } else {
            m_ActiveCRU = 0;
            m_Cartridge = dev->GetROM ();
            dev->DeActivate ();
            RemoveCartridge ( dev->GetROM (), false );
        }
        m_Cartridge = ctg;
    } else {
        dev->WriteCRU (( UINT16 ) (( address - dev->GetCRU ()) >> 1 ), val );
    }
}

void cTI994A::Run ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Run", true );

    m_CPU->Run ();
}

bool cTI994A::Step ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Step", true );

    return m_CPU->Step ();
}

void cTI994A::Stop ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Stop", true );

    m_CPU->Stop ();
}

bool cTI994A::IsRunning ()
{
    FUNCTION_ENTRY ( this, "cTI994A::IsRunning", true );

    return m_CPU->IsRunning ();
}

static const char ImageFileHeader[] = "TI-994/A Memory Image File\n\x1A";

bool cTI994A::OpenImageFile ( const char *filename, sImageFileState *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::OpenImageFile", true );

    if ( filename == NULL ) return false;

    info->file = fopen ( filename, "rb" );
    if ( info->file == NULL ) return false;

    char buffer [ sizeof ( ImageFileHeader ) - 1 ];
    if ( fread ( buffer, sizeof ( buffer ), 1, info->file ) != 1 ) {
        DBG_ERROR ( "Invalid memory image file" );
        fclose ( info->file );
        return false;
    }

    // Make sure it's a proper memory image file
    if ( memcmp ( buffer, ImageFileHeader, sizeof ( buffer )) != 0 ) {
        DBG_ERROR ( "Invalid memory image file" );
        fclose ( info->file );
        return false;
    }

    info->start = info->next = ftell ( info->file );

    return true;
}

bool cTI994A::FindHeader ( sImageFileState *info, HEADER_SECTION_E section )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::FindHeader", true );

    // Do a simple sanity check
    long offset = ftell ( info->file );
    if ( offset != info->next ) {
        DBG_WARNING ( "Incorrect offset " << hex << offset << " - expected " << info->next );
    }

    // Go to the 1st 'unclaimed' section
    fseek ( info->file, info->start, SEEK_SET );

    sStateHeader header;
    memset ( &header, -1, sizeof ( header ));

    do {
        offset = ftell ( info->file );
        if ( fread ( &header, sizeof ( header ), 1, info->file ) != 1 ) break;
        if ( header.id == section ) {
            info->next = offset + sizeof ( header ) + header.length;
            if ( offset == info->start ) info->start = info->next;
            return true;
        }
        fseek ( info->file, header.length, SEEK_CUR );
    } while ( ! feof ( info->file ));

    DBG_ERROR ( "Header " << section << " not found" );

    fseek ( info->file, info->next, SEEK_SET );

    return false;
}

void cTI994A::MarkHeader ( FILE *file, HEADER_SECTION_E section, sStateHeaderInfo *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::MarkHeader", true );

    info->header.id     = ( UINT16 ) section;
    info->header.length = ( UINT16 ) -section;
    info->offset        = ( UINT32 ) ftell ( file );

    fwrite ( &info->header, sizeof ( info->header ), 1, file );
}

bool cTI994A::SaveHeader ( FILE *file, sStateHeaderInfo *info )
{
    FUNCTION_ENTRY ( NULL, "cTI994A::SaveHeader", true );

    if (( UINT8 ) info->header.id != ( UINT8 ) -info->header.length ) {
        DBG_ERROR ( "Invalid header" );
        return false;
    }

    long offset = ftell ( file );
    info->header.length = ( UINT16 ) ( offset - info->offset - sizeof ( info->header ));

    fseek ( file, info->offset, SEEK_SET );
    if ( fwrite ( &info->header, sizeof ( info->header ), 1, file ) != 1 ) {
        DBG_ERROR ( "Unable to write file header to file" );
        return false;
    }
    fseek ( file, offset, SEEK_SET );

    return true;
}

bool cTI994A::SaveImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cTI994A::SaveImage", true );

    char buffer [256];
    snprintf ( buffer, sizeof ( buffer ), "%s%c%s", HOME_PATH, FILE_SEPERATOR, filename );

    FILE *file = fopen ( buffer, "wb" );
    if ( file == NULL ) {
        DBG_ERROR ( "Unable to open file '"<< buffer << "'" );
        return false;
    }

    fwrite ( ImageFileHeader, 1, sizeof ( ImageFileHeader ) - 1, file );

    sStateHeaderInfo info;
    MarkHeader ( file, SECTION_BASE, &info );

    // Name of inserted Cartridge
    if ( m_Cartridge && m_Cartridge->Title ()) {
        short len = ( short ) strlen ( m_Cartridge->Title ());
        fwrite ( &len, sizeof ( len ), 1, file );
        fwrite ( m_Cartridge->Title (), len, 1, file );
    } else {
        short len = 0;
        fwrite ( &len, sizeof ( len ), 1, file );
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_CPU, &info );

    m_CPU->SaveImage ( file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_VDP, &info );

    m_VDP->SaveImage ( file );

    UINT32 LastRetrace = m_CPU->GetClocks () - m_LastRetrace;
    fwrite ( &LastRetrace, sizeof ( LastRetrace ), 1, file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_GROM, &info );

    fwrite ( &m_GromAddress, sizeof ( m_GromAddress ), 1, file );
    fwrite ( &m_GromLastInstruction, sizeof ( m_GromLastInstruction ), 1, file );
    fwrite ( &m_GromReadShift, sizeof ( m_GromReadShift ), 1, file );
    fwrite ( &m_GromWriteShift, sizeof ( m_GromWriteShift ), 1, file );
    fwrite ( &m_GromCounter, sizeof ( m_GromCounter ), 1, file );

    for ( unsigned i = 0; i < 8; i++ ) {
        sMemoryRegion *memory = m_GromMemoryInfo [ i ];
        if ( memory == NULL ) continue;
        fputc ( memory->CurBank - memory->Bank, file );
        // Save the current bank of GRAM
        if ( memory->CurBank->Type != BANK_ROM ) {
            SaveBuffer ( GROM_BANK_SIZE, &m_GromMemory [ i * GROM_BANK_SIZE ], file );
        }
        // Save any other banks of GRAM
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if ( memory->Bank[j].Type == BANK_ROM ) continue;
            if ( memory->Bank[j].Data == NULL ) continue;
            if ( memory->CurBank == &memory->Bank [j] ) continue;
            SaveBuffer ( GROM_BANK_SIZE, memory->Bank[j].Data, file );
        }
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_CRU, &info );

    for ( unsigned i = 0; i < SIZE ( m_Device ); i++ ) {
        cDevice *dev = m_Device [i];
        if ( dev != NULL ) {
            fputc ( i, file );
            long oldOffset = ftell ( file );
            UINT16 size = 0;
            fwrite ( &size, sizeof ( size ), 1 , file );
            dev->SaveImage ( file );
            long newOffset = ftell ( file );
            size = ( UINT16 ) ( newOffset - oldOffset - sizeof ( size ));
            fseek ( file, oldOffset, SEEK_SET );
            fwrite ( &size, sizeof ( size ), 1 , file );
            fseek ( file, newOffset, SEEK_SET );
        }
    }
    fputc (( UINT8 ) -1, file );

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_DSR, &info );

    fwrite ( &m_ActiveCRU, sizeof ( m_ActiveCRU ), 1, file );
    if ( m_ActiveCRU != 0 ) {
        cDevice *dev = GetDevice ( m_ActiveCRU );
        dev->SaveImage ( file );
    }

    SaveHeader ( file, &info );
    MarkHeader ( file, SECTION_ROM, &info );

    for ( unsigned i = 0; i < 16; i++ ) {
        sMemoryRegion *memory = m_CpuMemoryInfo [ i ];
        if ( memory == NULL ) {
            fputc ( 0, file );
            continue;
        }
        fputc ( memory->NumBanks, file );
        fputc ( memory->CurBank - memory->Bank, file );

        if ( memory->CurBank->Type == BANK_ROM ) {
            fputc ( 0, file );
        } else {
            fputc ( 1, file );
            SaveBuffer ( ROM_BANK_SIZE, &CpuMemory [ i << 12 ], file );
        }

        // Save any other banks of RAM
        for ( int j = 0; j < memory->NumBanks; j++ ) {
            if (( memory->Bank[j].Type == BANK_ROM ) || ( memory->Bank[j].Data == NULL )) {
                fputc ( 0, file );
            } else {
                fputc ( 1, file );
                SaveBuffer ( ROM_BANK_SIZE, memory->Bank[j].Data, file );
            }
        }
    }

    SaveHeader ( file, &info );

    fclose ( file );

    return true;
}

bool cTI994A::LoadImage ( const char *filename )
{
    FUNCTION_ENTRY ( this, "cTI994A::LoadImage", true );

    char buffer [256];
    snprintf ( buffer, sizeof ( buffer ), "%s%c%s", HOME_PATH, FILE_SEPERATOR, filename );

    sImageFileState info;
    if ( OpenImageFile ( buffer, &info ) == false ) {
        return false;
    }

    bool retVal = true;
    bool reset = false;

    try {
        if ( FindHeader ( &info, SECTION_BASE ) != true ) throw std::exception ();

        // Make sure cartridge(s) match those currently loaded
        short len;
        if ( fread ( &len, sizeof ( len ), 1, info.file ) != 1 ) throw std::exception ();
        bool ok = true;
        bool haveCartridge = ( m_Cartridge && m_Cartridge->Title ()) ? true : false;
        if ( len != 0 ) {
            char *nameBuffer = new char [ len + 1 ];
            if ( fread ( nameBuffer, len, 1, info.file ) != 1 ) {
                delete [] nameBuffer;
                throw std::exception ();
            }
            nameBuffer [len] = '\0';
            if (( haveCartridge == false ) || strnicmp ( nameBuffer, m_Cartridge->Title (), len )) ok = false;
            DBG_STATUS ( "Cartridge: " << nameBuffer );
            delete [] nameBuffer;
        } else {
            if ( haveCartridge == true ) ok = false;
        }
        if ( ! ok ) {
            DBG_ERROR ( "The image's cartridges don't match current system" );
            fclose ( info.file );
            return false;
        }

        if ( FindHeader ( &info, SECTION_CPU ) != true ) throw std::exception ();

        // From here on, the system state may be compromised if an error occurs
        reset = true;

        if ( m_CPU->LoadImage ( info.file ) != true ) throw std::exception ();

        if ( FindHeader ( &info, SECTION_VDP ) != true) throw std::exception ();

        if ( m_VDP->LoadImage ( info.file ) != true ) throw std::exception ();

        UINT32 LastRetrace = 0;
        if ( fread ( &LastRetrace, sizeof ( LastRetrace ), 1, info.file ) != 1 ) throw std::exception ();
        m_LastRetrace = m_CPU->GetClocks () - LastRetrace;

        if ( FindHeader ( &info, SECTION_GROM ) != true ) throw std::exception ();

        if (( fread ( &m_GromAddress, sizeof ( m_GromAddress ), 1, info.file ) != 1 )                 ||
            ( fread ( &m_GromLastInstruction, sizeof ( m_GromLastInstruction ), 1, info.file ) != 1 ) ||
            ( fread ( &m_GromReadShift, sizeof ( m_GromReadShift ), 1, info.file ) != 1 )             ||
            ( fread ( &m_GromWriteShift, sizeof ( m_GromWriteShift ), 1, info.file ) != 1 )           ||
            ( fread ( &m_GromCounter, sizeof ( m_GromCounter ), 1, info.file ) != 1 )) throw std::exception ();

        m_GromPtr = &m_GromMemory [ m_GromAddress ];

        for ( unsigned i = 0; i < 8; i++ ) {
            sMemoryRegion *memory = m_GromMemoryInfo [ i ];
            if ( memory == NULL ) continue;
            UINT8 bank = ( UINT8 ) fgetc ( info.file );
            memory->CurBank = &memory->Bank [ bank ];
            if ( memory->CurBank->Type != BANK_ROM ) {
                LoadBuffer ( GROM_BANK_SIZE, &m_GromMemory [ i * GROM_BANK_SIZE ], info.file );
            }
            for ( int j = 0; j < memory->NumBanks; j++ ) {
                if ( memory->Bank[j].Type == BANK_ROM ) continue;
                if ( memory->Bank[j].Data == NULL ) continue;
                if ( memory->CurBank == &memory->Bank [j] ) continue;
                LoadBuffer ( GROM_BANK_SIZE, memory->Bank[j].Data, info.file );
            }
        }

        if ( FindHeader ( &info, SECTION_CRU ) != true ) throw std::exception ();

        for ( EVER ) {
            int i = fgetc ( info.file );
            if ( i == ( UINT8 ) -1 ) break;
            if ( ( i < 0 ) || ( i >= 32 ) ) throw std::exception ();
            UINT16 size;
            if ( fread ( &size, sizeof ( size ), 1, info.file ) != 1 ) throw std::exception ();
            long offset = ftell ( info.file );
            cDevice *dev = m_Device [i];
            if ( dev != NULL ) {
                dev->LoadImage ( info.file );
            }
            fseek ( info.file, offset + size, SEEK_SET );
        }

        if ( FindHeader ( &info, SECTION_DSR ) != true ) throw std::exception ();

        if ( m_ActiveCRU ) {
            cDevice *dev = GetDevice ( m_ActiveCRU );
            dev->DeActivate ();
            cCartridge *ctg = m_Cartridge;
            m_Cartridge = dev->GetROM ();
            RemoveCartridge ( dev->GetROM (), false );
            m_Cartridge = ctg;
        }
        if ( fread ( &m_ActiveCRU, sizeof ( m_ActiveCRU ), 1, info.file ) != 1 ) throw std::exception ();
        if ( m_ActiveCRU ) {
            cDevice *dev = GetDevice ( m_ActiveCRU );
            dev->LoadImage ( info.file );
            cCartridge *ctg = m_Cartridge;
            m_Cartridge = NULL;
            InsertCartridge ( dev->GetROM (), false );
            dev->Activate ();
            m_Cartridge = ctg;
        }

        if ( FindHeader ( &info, SECTION_ROM ) != true ) throw std::exception ();

        for ( unsigned i = 0; i < 16; i++ ) {
            fgetc ( info.file );
            sMemoryRegion *memory = m_CpuMemoryInfo [ i ];
            if ( memory == NULL ) continue;
            UINT8 bank = ( UINT8 ) fgetc ( info.file );
            memory->CurBank = &memory->Bank [ bank ];

            if ( fgetc ( info.file ) == 1 ) {
                LoadBuffer ( ROM_BANK_SIZE, &CpuMemory [ i << 12 ], info.file );
            }

            for ( int j = 0; j < memory->NumBanks; j++ ) {
                if ( fgetc ( info.file ) == 0 ) continue;
                LoadBuffer ( ROM_BANK_SIZE, memory->Bank[j].Data, info.file );
            }
        }

        Refresh ( true );
    }
    catch ( const std::exception & )
    {
        if ( reset ) {
            DBG_ERROR ( "Encountered an error while loading the image - reseting the system" );
            Reset ();
        }
        retVal = false;
    }

    fclose ( info.file );

    return retVal;
}

void cTI994A::Reset ()
{
    FUNCTION_ENTRY ( this, "cTI994A::Reset", true );

    if ( m_CPU != NULL ) m_CPU->Reset ();
    if ( m_VDP != NULL ) m_VDP->Reset ();
    if ( m_SpeechSynthesizer != NULL ) m_SpeechSynthesizer->Reset ();
}

void cTI994A::AddDevice ( cDevice *dev )
{
    FUNCTION_ENTRY ( this, "cTI994A::AddDevice", true );

    int index = ( dev->GetCRU () >> 8 ) & 0x1F;
    if ( m_Device [index] != NULL ) {
        DBG_WARNING ( "A CRU device already exists at address " << index );
        return;
    }
    m_Device [index] = dev;
    dev->SetCPU ( m_CPU );
}

void cTI994A::InsertCartridge ( cCartridge *cartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cTI994A::InsertCartridge", true );

    if ( m_Cartridge != NULL ) {
        cCartridge *ctg = m_Cartridge;
        RemoveCartridge ( ctg );
        delete ctg;
    }

    if ( cartridge == NULL ) return;
    m_Cartridge = cartridge;

    for ( unsigned i = 0; i < SIZE ( m_CpuMemoryInfo ); i++ ) {
        if ( m_Cartridge->CpuMemory [i].NumBanks > 0 ) {
            m_CpuMemoryInfo [i] = &m_Cartridge->CpuMemory [i];
            m_CpuMemoryInfo [i]->CurBank = &m_CpuMemoryInfo [i]->Bank[0];
            if ( m_CpuMemoryInfo [i]->NumBanks > 1 ) {
                UINT8 index = m_CPU->GetTrapIndex ( TrapFunction, TRAP_BANK_SWITCH );
                UINT16 address = ( UINT16 ) ( i << 12 );
                for ( unsigned j = 0; j < ROM_BANK_SIZE; j++ ) {
                    m_CPU->SetTrap ( address++, MEMFLG_TRAP_WRITE, index );
                }
            }
            MEMORY_TYPE_E memType = ( m_CpuMemoryInfo [i]->CurBank->Type == BANK_ROM ) ? MEM_ROM : MEM_RAM;
            m_CPU->SetMemory ( memType, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            memcpy ( &CpuMemory [ i << 12 ], m_CpuMemoryInfo [i]->CurBank->Data, ROM_BANK_SIZE );
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
        if ( m_Cartridge->GromMemory[i].NumBanks > 0 ) {
            m_GromMemoryInfo [i] = &m_Cartridge->GromMemory [i];
            m_GromMemoryInfo [i]->CurBank = &m_GromMemoryInfo [i]->Bank[0];
            memcpy ( &m_GromMemory [ i << 13 ], m_GromMemoryInfo [i]->CurBank->Data, GROM_BANK_SIZE );
        }
    }

    if ( reset ) Reset ();
}

void cTI994A::RemoveCartridge ( cCartridge *cartridge, bool reset )
{
    FUNCTION_ENTRY ( this, "cTI994A::RemoveCartridge", true );

    if ( cartridge != m_Cartridge ) return;

    // Save any battery-backed RAM to the cartridge and disabel bank-switched regions
    if ( m_Cartridge != NULL ) {
        for ( unsigned i = 0; i < SIZE ( m_CpuMemoryInfo ); i++ ) {
            if ( m_Cartridge->CpuMemory [i].NumBanks == 0 ) continue;
            // If this bank is RAM & Battery backed - update the cartridge
            if ( m_Cartridge->CpuMemory [i].CurBank->Type == BANK_BATTERY_BACKED ) {
                memcpy ( m_CpuMemoryInfo [i]->CurBank->Data, &CpuMemory [ i << 12 ], ROM_BANK_SIZE );
            }
            if ( m_Cartridge->CpuMemory [i].NumBanks > 1 ) {
                // Clears bankswitch breakpoint for ALL regions!
                UINT8 index = m_CPU->GetTrapIndex ( TrapFunction, TRAP_BANK_SWITCH );
                m_CPU->ClearTrap ( index );
            }
        }

        for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
            if ( m_Cartridge->GromMemory [i].NumBanks == 0 ) continue;
            // If this bank is RAM & Battery backed - update the cartridge
            if ( m_Cartridge->GromMemory [i].CurBank->Type == BANK_BATTERY_BACKED ) {
                memcpy ( m_GromMemoryInfo [i]->CurBank->Data, &m_GromMemory [ i << 13 ], GROM_BANK_SIZE );
            }
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_Cartridge->CpuMemory ); i++ ) {
        if (( m_Cartridge != NULL ) && ( m_Cartridge->CpuMemory [i].NumBanks == 0 )) continue;
        if (( m_Console != NULL ) && ( m_Console->CpuMemory[i].CurBank != NULL )) {
            MEMORY_TYPE_E memType = ( m_Console->CpuMemory[i].CurBank->Type == BANK_ROM ) ? MEM_ROM : MEM_RAM;
            m_CPU->SetMemory ( memType, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            m_CpuMemoryInfo [i] = &m_Console->CpuMemory [i];
            // Don't clear out memory!?
            memcpy ( &CpuMemory [ i << 12 ], m_CpuMemoryInfo [i]->CurBank->Data, ROM_BANK_SIZE );
        } else {
            m_CpuMemoryInfo [i] = NULL;
            m_CPU->SetMemory ( MEM_ROM, ( ADDRESS ) ( i << 12 ), ROM_BANK_SIZE );
            memset ( &CpuMemory [ i << 12 ], 0, ROM_BANK_SIZE );
        }
    }

    for ( unsigned i = 0; i < SIZE ( m_GromMemoryInfo ); i++ ) {
        if (( m_Cartridge != NULL ) && ( m_Cartridge->GromMemory [i].NumBanks == 0 )) continue;
        if (( m_Console != NULL ) && ( m_Console->GromMemory[i].CurBank != NULL )) {
            m_GromMemoryInfo [i] = &m_Console->GromMemory [i];
            memcpy ( &m_GromMemory [ i << 13 ], m_GromMemoryInfo [i]->CurBank->Data, GROM_BANK_SIZE );
        } else {
            m_GromMemoryInfo [i] = NULL;
            memset ( &m_GromMemory [ i << 13 ], 0, GROM_BANK_SIZE );
        }
    }

    m_Cartridge = NULL;

    if ( reset ) Reset ();
}
