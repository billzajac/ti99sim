//----------------------------------------------------------------------------
//
// File:        ti-disk.cpp
// Date:        27-Mar-1998
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
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "device.hpp"
#include "diskio.hpp"
#include "ti-disk.hpp"

DBG_REGISTER ( __FILE__ );

cDiskDevice::cDiskDevice ( const char *filename ) :
    cDevice ( filename ),
    m_StepDirection ( 0 ),
    m_ClocksPerRev ( 600000 ),
    m_ClockStart ( 0 ),
    m_HardwareBits ( 0 ),
    m_DriveSelect ( 0 ),
    m_HeadSelect ( 0 ),
    m_TrackSelect ( 0 ),
    m_IsFD1771 ( true ),
    m_TransferEnabled ( false ),
    m_CurDisk ( NULL ),
    m_CurSector ( NULL ),
    m_StatusRegister ( 0 ),
    m_TrackRegister ( 0 ),
    m_SectorRegister ( 0 ),
    m_LastData ( 0x00 ),
    m_BytesExpected ( 0 ),
    m_BytesLeft ( 0 ),
    m_DataPtr ( NULL ),
    m_CmdInProgress ( CMD_NONE ),
    m_TrapIndex (( UINT8 ) -1 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::cDiskDevice", true );

    for ( unsigned i = 0; i < SIZE ( m_DiskMedia ); i++ ) {
        m_DiskMedia [i] = new cDiskMedia ();
    }

    memset ( m_DataBuffer, 0, sizeof ( m_DataBuffer ));

    if ( m_IsValid && ( m_CRU == -1 )) {
        DBG_ERROR ( "Cartridge does not appear to be a valid disk device" );
        m_IsValid = false;
    }
}

cDiskDevice::~cDiskDevice ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::~cDiskDevice", true );

    for ( unsigned i = 0; i < SIZE ( m_DiskMedia ); i++ ) {
        m_DiskMedia [i]->Release ( NULL );
    }
}

void cDiskDevice::Activate ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::Activate", true );

    // Make sure we have a valid DSR before going any further
    if ( m_IsValid == false ) return;

    if ( m_TrapIndex != ( UINT8 ) -1 ) return;

    m_TrapIndex = m_pCPU->RegisterTrapHandler ( TrapFunction, this, TRAP_DISK );

    m_pCPU->SetTrap ( 0x5FF0, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FF2, ( UINT8 ) ( MEMFLG_TRAP_READ | MEMFLG_TRAP_WRITE ), m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FF4, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FF6, ( UINT8 ) MEMFLG_TRAP_READ,  m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FF8, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FFA, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FFC, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );
    m_pCPU->SetTrap ( 0x5FFE, ( UINT8 ) MEMFLG_TRAP_WRITE, m_TrapIndex );

/*
    m_pCPU->SetMemory ( MEM_ROM, 0x5020, 0x6000 - 0x5020 );

    for ( int i = 0x5020; i < 0x6000; i += 1 ) {
        m_pCPU->SetTrap ( i, ( UINT8 ) ( MEMFLG_TRAP_READ | MEMFLG_TRAP_WRITE ), m_TrapIndex );
    }
*/
}

void cDiskDevice::DeActivate ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::DeActivate", true );

    if ( m_TrapIndex != ( UINT8 ) -1 ) {
        m_pCPU->DeRegisterTrapHandler ( m_TrapIndex );
        m_TrapIndex = ( UINT8 ) -1;
    }
}

void cDiskDevice::WriteCRU ( ADDRESS address, int val )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::WriteCRU", true );

    int mask = 1 << address;
    if ( val != 0 ) m_HardwareBits |= mask;
    else m_HardwareBits &= ~mask;

    switch ( address ) {
        case 1 :
            // Don't care - triggers a 4.23 second pulse to the selected drive's motor
            break;
        case 2 :
            m_TransferEnabled = ( val != 0 ) ? true : false;
            DBG_EVENT ( "Transfer " << (( val != 0 ) ? "enabled" : "disabled" ));
            return;
        case 3 :
            // Don't care - head load
            break;
        case 4 :					// Bits 4,5,6
        case 5 :
            return;
        case 6 :
            m_DriveSelect = ( m_HardwareBits >> 4 ) & 0x07;
            int drive;
            switch ( m_DriveSelect ) {
                case 0x01 : m_CurDisk = m_DiskMedia [drive=0];  break;
                case 0x02 : m_CurDisk = m_DiskMedia [drive=1];  break;
                case 0x04 : m_CurDisk = m_DiskMedia [drive=2];  break;
                default :   m_CurDisk = NULL;  drive = -1;      break;
            }
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Drive " << dec << drive << " selected" );
            return;
        case 7 :
            m_HeadSelect = ( UINT8 ) val;
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Head " << dec << val << " selected" );
            return;
        case 11 :
//            return;
        default:
            DBG_ERROR ( "PC: " << hex << m_pCPU->GetPC () << " Unexpected Address - " << hex << address );
            break;
    }

////    DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << "   I/O Wr: " << hex << ( UINT8 ) address << " => " << dec << val );
}

int cDiskDevice::ReadCRU ( ADDRESS address )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadCRU", true );

    int retVal = 1;
    switch ( address ) {
        case 7 :
            // Matches output pin 7
            retVal = m_HeadSelect;
            break;
        case 6 :
            // Tied to +5V
            break;
        case 5 :
            // Ground
            retVal = 0;
            break;
        case 3 :
            retVal = ( m_HardwareBits & 0x40 ) ? 1 : 0;
            break;
        case 2 :
            retVal = ( m_HardwareBits & 0x20 ) ? 1 : 0;
            break;
        case 1 :
            retVal = ( m_HardwareBits & 0x10 ) ? 1 : 0;
            break;
        default:
            DBG_ERROR ( "Unexpected Address - " << address );
            break;
    }

////    DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << "   I/O Rd: " << hex << ( UINT8 ) address << " => " << dec << retVal );

    return retVal;
}

bool cDiskDevice::SaveImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::SaveImage", true );

    if (( fwrite ( &m_StatusRegister, sizeof ( m_StatusRegister ), 1, file ) != 1 ) ||
        ( fwrite ( &m_LastData, sizeof ( m_LastData ), 1, file ) != 1 )             ||
        ( fwrite ( &m_HardwareBits, sizeof ( m_HardwareBits ), 1, file ) != 1 )     ||
        ( fwrite ( &m_StepDirection, sizeof ( m_StepDirection ), 1, file ) != 1 )   ||
        ( fwrite ( &m_DriveSelect, sizeof ( m_DriveSelect ), 1, file ) != 1 )       ||
        ( fwrite ( &m_HeadSelect, sizeof ( m_HeadSelect ), 1, file ) != 1 )         ||
        ( fwrite ( &m_TrackSelect, sizeof ( m_TrackSelect ), 1, file ) != 1 )       ||
        ( fwrite ( &m_TrackRegister, sizeof ( m_TrackRegister ), 1, file ) != 1 )   ||
        ( fwrite ( &m_SectorRegister, sizeof ( m_SectorRegister ), 1, file ) != 1 ) ||
        ( fwrite ( &m_TransferEnabled, sizeof ( m_TransferEnabled ), 1, file ) != 1 )) {
        DBG_ERROR ( "Error saving image to file" );
        return false;
    }

    for ( unsigned i = 0; i < SIZE ( m_DiskMedia ); i++ ) {
        const char *name = m_DiskMedia [i]->GetName ();
        if ( name != NULL ) {
            if (( fputc (( int ) strlen ( name ), file ) == EOF ) ||
                ( fwrite ( name, strlen ( name ), 1, file ) != 1 )) {
                DBG_ERROR ( "Error saving image to file" );
                return false;
            }
        } else {
            if ( fputc ( 0, file ) == EOF ) {
                DBG_ERROR ( "Error saving image to file" );
                return false;
            }
        }
    }

    if (( fwrite ( &m_CmdInProgress, sizeof ( m_CmdInProgress ), 1, file ) != 1 ) ||
        ( fwrite ( &m_BytesExpected, sizeof ( m_BytesExpected ), 1, file ) != 1 ) ||
        ( fwrite ( &m_BytesLeft, sizeof ( m_BytesLeft ), 1, file ) != 1 )         ||
        ( fwrite ( m_DataBuffer, m_BytesExpected, 1, file ) != 1 )) {
        DBG_ERROR ( "Error saving image to file" );
        return false;
    }

    return true;
}

bool cDiskDevice::LoadImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::LoadImage", true );

    if (( fread ( &m_StatusRegister, sizeof ( m_StatusRegister ), 1, file ) != 1 ) ||
        ( fread ( &m_LastData, sizeof ( m_LastData ), 1, file ) != 1 )             ||
        ( fread ( &m_HardwareBits, sizeof ( m_HardwareBits ), 1, file ) != 1 )     ||
        ( fread ( &m_StepDirection, sizeof ( m_StepDirection ), 1, file ) != 1 )   ||
        ( fread ( &m_DriveSelect, sizeof ( m_DriveSelect ), 1, file ) != 1 )       ||
        ( fread ( &m_HeadSelect, sizeof ( m_HeadSelect ), 1, file ) != 1 )         ||
        ( fread ( &m_TrackSelect, sizeof ( m_TrackSelect ), 1, file ) != 1 )       ||
        ( fread ( &m_TrackRegister, sizeof ( m_TrackRegister ), 1, file ) != 1 )   ||
        ( fread ( &m_SectorRegister, sizeof ( m_SectorRegister ), 1, file ) != 1 ) ||
        ( fread ( &m_TransferEnabled, sizeof ( m_TransferEnabled ), 1, file ) != 1 )) {
        DBG_ERROR ( "Error loading image from file" );
        return false;
    }

    for ( unsigned i = 0; i < SIZE ( m_DiskMedia ); i++ ) {
        int length = fgetc ( file );
        if ( length == EOF ) {
            DBG_ERROR ( "Error loading image from file" );
            return false;
        } else if ( length != 0 ) {
            char *name = new char [ length + 1 ];
            if ( fread ( name, length, 1, file ) != 1 ) {
                DBG_ERROR ( "Error loading image from file" );
                delete [] name;
                return false;
            }
            name [length] = '\0';
            m_DiskMedia [i]->LoadFile ( name );
            delete [] name;
        } else {
            m_DiskMedia [i]->ClearDisk ();
        }
    }

    if (( fread ( &m_CmdInProgress, sizeof ( m_CmdInProgress ), 1, file ) != 1 ) ||
        ( fread ( &m_BytesExpected, sizeof ( m_BytesExpected ), 1, file ) != 1 ) ||
        ( fread ( &m_BytesLeft, sizeof ( m_BytesLeft ), 1, file ) != 1 )         ||
        ( fread ( m_DataBuffer, m_BytesExpected, 1, file ) != 1 )) {
        DBG_ERROR ( "Error loading image from file" );
        return false;
    }

    // Now update derived member variables

    switch ( m_DriveSelect ) {
        case 0x01 : m_CurDisk = m_DiskMedia [0];  break;
        case 0x02 : m_CurDisk = m_DiskMedia [1];  break;
        case 0x04 : m_CurDisk = m_DiskMedia [2];  break;
        default :   m_CurDisk = NULL;             break;
    }

    FindSector ();

    m_DataPtr = m_DataBuffer + ( m_BytesExpected - m_BytesLeft );

    return true;
}

void cDiskDevice::LoadDisk ( int index, const char *filename )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::LoadDisk", true );

    DBG_EVENT ( "Loading file: " << filename );

    if ( m_DiskMedia [index]->LoadFile ( filename ) == true ) {
        DBG_EVENT ( "Disk image loaded successfully" );
    }
}

void cDiskDevice::UnLoadDisk ( int index )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::UnLoadDisk", true );

    DBG_EVENT ( "Removing disk: " << m_DiskMedia [index]->GetName ());

    m_DiskMedia [index]->ClearDisk ();
}

UINT8 cDiskDevice::TrapFunction ( void *ptr, int, bool read, ADDRESS address, UINT8 value )
{
    FUNCTION_ENTRY ( ptr, "cDiskDevice::Trap", false );

    cDiskDevice *pThis = ( cDiskDevice * ) ptr;

    if ( read == true ) {
        value = pThis->ReadMemory ( address, value );
    } else {
        value = pThis->WriteMemory ( address, value );
    }

    return value;
}

void cDiskDevice::FindSector ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::FindSector", true );

    if ( m_CurDisk != NULL ) {
        m_CurSector = m_CurDisk->GetSector ( m_TrackSelect, m_HeadSelect, m_SectorRegister, m_TrackRegister );

        if ( m_CurSector == NULL ) {
            DBG_ERROR ( "PC: " << hex << m_pCPU->GetPC () << " ------ Unable to find sector " << m_SectorRegister << " on track " << m_TrackRegister << " side " << m_HeadSelect << "! -------" );
        }
    }

}

void cDiskDevice::CompleteCommand ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::CompleteCommand", true );

    m_ClockStart = 0;

    switch ( m_CmdInProgress ) {
        case CMD_NONE :
            break;
        case CMD_READ_ADDRESS :
            break;
        case CMD_READ_TRACK :
            break;
        case CMD_READ_SECTOR :
            break;
        case CMD_WRITE_TRACK :
            m_StatusRegister |= STATUS_LOST_DATA;
            m_CurDisk->WriteTrack ( m_TrackSelect, m_HeadSelect, m_BytesExpected - m_BytesLeft, m_DataBuffer );
            break;
        case CMD_WRITE_SECTOR :
            m_StatusRegister |= STATUS_LOST_DATA;
            m_CurDisk->WriteSector ( m_TrackSelect, m_HeadSelect, m_CurSector->LogicalSector, m_CurSector->LogicalCylinder, m_DataBuffer );
            break;
    }

    m_CmdInProgress = CMD_NONE;

    m_StatusRegister &= ~STATUS_BUSY;
}

UINT8 cDiskDevice::ReadByte ()
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadByte", true );

    UINT8 retVal = 0;

    if ( m_BytesLeft > 0 ) {
        if ( --m_BytesLeft == 0 ) {
            m_StatusRegister &= ~STATUS_BUSY;
        }
        retVal = *m_DataPtr++;
    } else {
        ; // Set error?
    }

    return retVal;
}

void cDiskDevice::WriteByte ( UINT8 val )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::WriteByte", true );

    m_LastData = val;

    if ( m_BytesLeft > 0 ) {

        if ( m_CmdInProgress == CMD_WRITE_TRACK ) {
            switch ( val ) {
                case 0xF5 :                 // Preset the CRC
                    if ( 0 ) val = 0xA1;
                    break;
                case 0xF6 :
                    if ( 0 ) val = 0xC2;
                    break;
                case 0xF7 :                 // Insert CRC
                    *m_DataPtr++ = 0xF7;
                    m_BytesLeft--;
                    break;
                default :
                    break;
            }
        }
        *m_DataPtr++ = val;

        if ( --m_BytesLeft == 0 ) {
            m_StatusRegister &= ~STATUS_BUSY;
            if ( m_CmdInProgress == CMD_WRITE_TRACK ) {
                m_CurDisk->WriteTrack ( m_TrackSelect, m_HeadSelect, m_BytesExpected, m_DataBuffer );
            } else {
                m_CurDisk->WriteSector ( m_TrackSelect, m_HeadSelect, m_CurSector->LogicalSector, m_CurSector->LogicalCylinder, m_DataBuffer );
            }
        }
    } else {
        ; // Just ignore these
    }
}

void cDiskDevice::Restore ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::Restore", true );

    m_TrackSelect    = 0;
    m_TrackRegister  = 0;

    m_SectorRegister = 0;

    // Set the status register
    m_StatusRegister = STATUS_TRACK_0;

    // Check for track verification
    if ( cmd & 0x04 ) {
        DBG_EVENT ( "Verifing track" );
        const sTrack *track = m_CurDisk->GetTrack ( m_TrackSelect, m_HeadSelect );
        if (( track == NULL ) || ( track->Sector [0].LogicalCylinder != 0 )) {
            DBG_EVENT ( "Track verification failed" );
            m_StatusRegister |= STATUS_SEEK_ERROR;
        }
    }
}

void cDiskDevice::Seek ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::Seek", true );

    // Assume the Track Register is correct and move accordingly
    m_TrackSelect   += ( m_LastData - m_TrackRegister );
    m_TrackRegister  = m_LastData;

    // Keep the hardware track within range
    if ( m_TrackSelect >= MAX_TRACKS ) m_TrackSelect = MAX_TRACKS - 1;

    DBG_TRACE ( "Seeking to track " << m_TrackSelect );

    // Set the status register
    m_StatusRegister = ( m_TrackSelect == 0 ) ? STATUS_TRACK_0 : 0x00;

    // Check for track verification
    if ( cmd & 0x04 ) {
        DBG_EVENT ( "Verifing track" );
        const sTrack *track = m_CurDisk->GetTrack ( m_TrackSelect, m_HeadSelect );
        if (( track == NULL ) || ( track->Sector [0].LogicalCylinder != m_TrackRegister )) {
            DBG_EVENT ( "Track verification failed" );
            m_StatusRegister |= STATUS_SEEK_ERROR;
        }
    }
}

void cDiskDevice::Step ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::Step", true );

    m_TrackSelect += m_StepDirection;

    // Keep the hardware track within range
    if ( m_TrackSelect >= MAX_TRACKS ) m_TrackSelect = ( m_StepDirection > 0 ) ? MAX_TRACKS - 1 : 0;

    DBG_TRACE ( "Stepping to track " << m_TrackSelect );

    // Update the track register
    if ( cmd & 0x10 ) {
        DBG_EVENT ( "Updating track register" );
        m_TrackRegister = m_TrackSelect;
    }

    // Set the status register
    m_StatusRegister = ( m_TrackSelect == 0 ) ? STATUS_TRACK_0 : 0x00;

    // Check for track verification
    if ( cmd & 0x04 ) {
        DBG_EVENT ( "Verifing track" );
        const sTrack *track = m_CurDisk->GetTrack ( m_TrackSelect, m_HeadSelect );
        if (( track == NULL ) || ( track->Sector [0].LogicalCylinder != m_TrackRegister )) {
            DBG_EVENT ( "Track verification failed" );
            m_StatusRegister |= STATUS_SEEK_ERROR;
        }
    }
}

void cDiskDevice::StepIn ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::StepIn", true );

    m_StepDirection = 1;

    Step ( cmd );
}

void cDiskDevice::StepOut ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::StepOut", true );

    m_StepDirection = -1;

    Step ( cmd );
}

void cDiskDevice::ReadSector ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadSector", true );

    FindSector ();

    if ( m_CurSector != NULL ) {
        DBG_EVENT ( " C:" << m_CurSector->LogicalCylinder			\
             << " H:" << m_CurSector->LogicalSide			\
             << " S:" << m_CurSector->LogicalSector			\
             << " L:" << m_CurSector->Size				\
             << " - " << dec << ( 128 << m_CurSector->Size ));
        m_BytesExpected   = 128 << m_CurSector->Size;
        m_BytesLeft       = 128 << m_CurSector->Size;
        m_DataPtr         = m_CurSector->Data;
        m_StatusRegister |= STATUS_BUSY;
        m_StatusRegister &= ~STATUS_NOT_FOUND;
        // Handle special Data Address Marks
        m_StatusRegister &= 0x60;
        if ( m_IsFD1771 == true ) {
            m_StatusRegister |= ( 0x03 ^ ( m_CurSector->Data [-1] & 0x03 )) << 5;
        } else {
            m_StatusRegister |= ( 0x02 ^ ( m_CurSector->Data [-1] & 0x02 )) << 4;
        }
        m_CmdInProgress   = CMD_READ_SECTOR;
    } else {
        ; // ?? set up a dummy buffer & error status ??
        m_StatusRegister |= STATUS_NOT_FOUND;
    }

    if ( cmd & 0x10 ) {
        DBG_WARNING ( "Multi-sector write requested - feature not implemented" );
    }
}

void cDiskDevice::WriteSector ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::WriteSector", true );

    if ( m_CurDisk->IsWriteProtected ()) return;

    FindSector ();

    if ( m_CurSector != NULL ) {
        DBG_EVENT ( " C:" << m_CurSector->LogicalCylinder			\
             << " H:" << m_CurSector->LogicalSide			\
             << " S:" << m_CurSector->LogicalSector			\
             << " L:" << m_CurSector->Size				\
             << " - " << dec << ( 128 << m_CurSector->Size ));

        m_BytesExpected   = 128 << m_CurSector->Size;
        m_BytesLeft       = 128 << m_CurSector->Size;
        m_DataPtr         = m_DataBuffer;
        m_StatusRegister |= STATUS_BUSY;
        m_StatusRegister &= ~STATUS_NOT_FOUND;
        // Set special Data Address Marks
        if ( m_IsFD1771 == true ) {
            m_CurSector->Data [-1] = 0xFB - ( cmd & 0x03 );
        } else {
            m_CurSector->Data [-1] = ( cmd & 0x01 ) ? 0xF8 : 0xFB;
        }
        m_CmdInProgress   = CMD_WRITE_SECTOR;
    } else {
        m_StatusRegister |= STATUS_NOT_FOUND;
    }

    if ( cmd & 0x10 ) {
        DBG_WARNING ( "Multi-sector write requested - feature not implemented" );
    }
}

void cDiskDevice::ReadAddress ( UINT8 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadAddress", true );

    FindSector ();

    if ( m_CurSector != NULL ) {
        m_DataBuffer [0] = m_CurSector->LogicalCylinder;
        m_DataBuffer [1] = m_CurSector->LogicalSide;
        m_DataBuffer [2] = m_CurSector->LogicalSector;
        m_DataBuffer [3] = m_CurSector->Size;
        m_DataBuffer [4] = 0xFF;			// CRC
        m_DataBuffer [5] = 0xFF;			// CRC

        m_BytesExpected   = 6;
        m_BytesLeft       = 6;
        m_DataPtr         = m_DataBuffer;
        m_StatusRegister |= STATUS_BUSY;
        m_StatusRegister &= ~STATUS_NOT_FOUND;

        m_CmdInProgress   = CMD_READ_ADDRESS;
    } else {
        m_StatusRegister |= STATUS_NOT_FOUND;
    }
}

void cDiskDevice::ReadTrack ( UINT8 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadTrack", true );

    m_ClockStart = m_pCPU->GetClocks ();

    const sTrack *track = m_CurDisk->GetTrack ( m_TrackSelect, m_HeadSelect );

    if (( track != NULL ) && ( track->Data != NULL )) {
        DBG_EVENT ( " T:" << m_TrackSelect << " H:" << m_HeadSelect );
        m_BytesExpected   = track->Size;
        m_BytesLeft       = track->Size;
        m_DataPtr         = track->Data;
        m_StatusRegister |= STATUS_BUSY;
        m_StatusRegister &= ~STATUS_NOT_FOUND;
        m_CmdInProgress   = CMD_READ_TRACK;
    } else {
        m_BytesExpected   = 0;
        m_BytesLeft       = 0;
        m_DataPtr         = NULL;
        m_StatusRegister |= STATUS_NOT_FOUND;
    }
}

void cDiskDevice::WriteTrack ( UINT8 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::WriteTrack", true );

    if ( m_CurDisk->IsWriteProtected ()) return;

    m_ClockStart = m_pCPU->GetClocks ();

    const sTrack *track = m_CurDisk->GetTrack ( m_TrackSelect, m_HeadSelect );

    if (( track != NULL ) && ( track->Data != NULL )) {
        DBG_EVENT ( " T:" << m_TrackSelect << " H:" << m_HeadSelect );
        m_BytesExpected   = track->Size ? track->Size : TRACK_SIZE_FM;
        m_BytesLeft       = m_BytesExpected;
        m_DataPtr         = m_DataBuffer;
        m_StatusRegister |= STATUS_BUSY;
        m_StatusRegister &= ~STATUS_NOT_FOUND;
        m_CmdInProgress   = CMD_WRITE_TRACK;
    } else {
        m_BytesExpected   = 0;
        m_BytesLeft       = 0;
        m_DataPtr         = NULL;
        m_StatusRegister |= STATUS_NOT_FOUND;
    }
}

void cDiskDevice::ForceInterrupt ( UINT8 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ForceInterrupt", true );

    m_StatusRegister &= ~STATUS_BUSY;
}

void cDiskDevice::HandleCommand ( UINT8 cmd )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::HandleCommand", true );

    // Make sure the previous command has completed
    CompleteCommand ();

    switch ( cmd & 0xF0 ) {
        case 0x00 :			// CMD_RESTORE
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_RESTORE" );
            Restore ( cmd );
            break;
        case 0x10 :			// CMD_SEEK
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_SEEK" );
            Seek ( cmd );
            break;
        case 0x20 :			// CMD_STEP
        case 0x30 :
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_STEP" );
            Step ( cmd );
            break;
        case 0x40 :			// CMD_STEP_IN
        case 0x50 :
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_STEP_IN" );
            StepIn ( cmd );
            break;
        case 0x60 :			// CMD_STEP_OUT
        case 0x70 :
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_STEP_OUT" );
            StepOut ( cmd );
            break;
        case 0x80 :			// CMD_READ
        case 0x90 :
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_READ" );
            ReadSector ( cmd );
            break;
        case 0xA0 :			// CMD_WRITE
        case 0xB0 :			// CMD_WRITE
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_WRITE" );
            WriteSector ( cmd );
            break;
        case 0xC0 :			// CMD_READ_ADDRESS
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_READ_ADDRESS" );
            ReadAddress ( cmd );
            break;
        case 0xD0 :			// CMD_INTERRUPT
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_INTERRUPT" );
            ForceInterrupt ( cmd );
            break;
        case 0xE0 :			// CMD_READ_TRACK
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_READ_TRACK" );
            ReadTrack ( cmd );
            break;
        case 0xF0 :			// CMD_WRITE_TRACK
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " => " << "CMD_WRITE_TRACK" );
            WriteTrack ( cmd );
            break;
        default :
            DBG_ERROR ( "PC: " << hex << m_pCPU->GetPC () << " Unknown command: " << hex << ( UINT8 ) ( cmd & 0xF0 ));
            return;
    }
}

UINT8 cDiskDevice::WriteMemory ( ADDRESS address, UINT8 val )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::WriteMemory", true );

    val ^= 0xFF;
    switch ( address ) {
        case REG_COMMAND :
            HandleCommand ( val );
            break;
        case REG_WR_TRACK :
            m_TrackRegister = val;
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Set Track = " << hex << val );
            break;
        case REG_WR_SECTOR :
            m_SectorRegister = val;
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Set Sector = " << hex << val );
            break;
        case REG_WR_DATA :
            WriteByte ( val );
            break;
        default :
            DBG_ERROR ( "PC: " << hex << m_pCPU->GetPC () << " *** Unexpected MEM Wr: " << hex << address << " => " << val );
    }

    return val;
}

UINT8 cDiskDevice::ReadMemory ( ADDRESS address, UINT8 )
{
    FUNCTION_ENTRY ( this, "cDiskDevice::ReadMemory", true );

    UINT8 retVal = 0xFF;

    switch ( address ) {
        case REG_STATUS :
            if (( m_ClockStart != 0 ) && ( m_pCPU->GetClocks () - m_ClockStart > m_ClocksPerRev )) {
                CompleteCommand ();
            }
            retVal = m_StatusRegister;
            if (( m_CurDisk != NULL ) && ( m_CurDisk->IsWriteProtected ())) {
                retVal |= STATUS_WRITE_PROTECTED;
            }
            if (( m_pCPU->GetClocks () % m_ClocksPerRev ) < 10 * m_ClocksPerRev / 360 ) {
                retVal |= STATUS_INDEX_PULSE;
            }
//            DBG_TRACE ( "Left: " << m_BytesLeft << " Expected: " << m_BytesExpected );
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Get Status = " << hex << ( UINT8 ) retVal );
            break;
        case REG_RD_TRACK :
            retVal = m_TrackSelect;
            DBG_EVENT ( "PC: " << hex << m_pCPU->GetPC () << " Get Track = " << hex << ( UINT8 ) retVal );
            break;
        case REG_RD_DATA :
            retVal = ReadByte ();
            break;
        default :
            DBG_ERROR ( "PC: " << hex << m_pCPU->GetPC () << " *** Unexpected MEM Rd: " << hex << address );
    }

    return ( UINT8 ) ( retVal ^ 0xFF );
}
