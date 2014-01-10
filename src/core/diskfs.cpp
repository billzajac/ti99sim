//----------------------------------------------------------------------------
//
// File:        diskfs.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description: A class to manage the filesystem information on a TI disk.
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

#if defined ( _MSC_VER )
    #include <malloc.h>
#else
    #include <alloca.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "common.hpp"
#include "logger.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "fileio.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

cDiskFileSystem *cDiskFileSystem::sm_SortDisk;

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem *cDiskFileSystem::Open ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "cDiskFileSystem::Open", true );

    int volume = -1;

    const char *actualFileName = LocateFile ( filename, path );

    if ( actualFileName == NULL ) {
        if ( strrchr ( filename, '#' ) != NULL ) {
            char *buffer = ( char * ) alloca ( strlen ( filename ) + 1 );
            strcpy ( buffer, filename );
            char *vol = strrchr ( buffer, '#' );
            *vol++ = '\0';
            errno = 0;
            long index = strtol ( vol, &vol, 10 );
            if (( errno == 0 ) && ( index > 0 ) && ( index < INT_MAX ) && ( *vol == '\0' )) {
                volume = index - 1;
                actualFileName = LocateFile ( buffer, path );
            }
        }
    }

    if ( actualFileName != NULL ) {
        cDiskMedia *media = new cDiskMedia ( actualFileName, volume );
        if ( media->GetFormat () != FORMAT_INVALID ) {
            cDiskFileSystem *disk = new cDiskFileSystem ( media );
            media->Release ( NULL );
            return disk;
        }
        media->Release ( NULL );
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem ctor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::cDiskFileSystem ( cDiskMedia *media ) :
    cFileSystem ( "cDiskFileSystem" ),
    m_Media ( media ),
    m_VIB ( NULL )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem ctor", true );

    if ( m_Media != NULL ) {
        m_Media->AddRef ( this );
        sSector *sec = m_Media->GetSector ( 0, 0, 0 );
        if ( sec != NULL ) {
            m_VIB = ( VIB * ) sec->Data;
        }
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem dtor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskFileSystem::~cDiskFileSystem ()
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem dtor", true );

    m_VIB = NULL;

    if ( m_Media != NULL ) {
        m_Media->Release ( this );
        m_Media = NULL;
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetMedia
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cDiskMedia *cDiskFileSystem::GetMedia ( iBaseObject *obj ) const
{
    if ( m_Media != NULL ) {
        m_Media->AddRef ( obj );
        return m_Media;
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFreeSector
// Purpose:     Find a free sector on the disk beginning at sector 'start'
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFreeSector ( int start ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindFreeSector", true );

    int index = ( start / 8 ) * 8;

    for ( unsigned i = ( start / 8 ); i < SIZE ( m_VIB->AllocationMap ); i++ ) {
        UINT8 bits = m_VIB->AllocationMap [i];
        for ( int j = 0; j < 8; j++ ) {
            if ((( bits & 1 ) == 0 ) && ( index >= start )) {
                return index;
            }
            index++;
            bits >>= 1;
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::SetSectorAllocation
// Purpose:     Update the allocation bitmap in the VIB for the indicated sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::SetSectorAllocation ( int index, bool bUsed )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::SetSectorAllocation", true );

    int i   = index / 8;
    int bit = index % 8;

    if ( bUsed == true ) {
        m_VIB->AllocationMap [i] |= 1 << bit;
    } else {
        m_VIB->AllocationMap [i] &= ~ ( 1 << bit );
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindLastSector
// Purpose:     Return the index of the last sector of the file
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindLastSector ( const sFileDescriptorRecord *FDR ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindLastSector", true );

    int totalSectors = GetUINT16 ( &FDR->TotalSectors );

    const CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        DBG_ASSERT ( offset > count );

        count = offset;
        chain++;
    }

    return count - 1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileSector
// Purpose:     Add the sector to the file's sector chain
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFileSector ( sFileDescriptorRecord *FDR, int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFileSector", true );

    int totalSectors = GetUINT16 ( &FDR->TotalSectors );

    CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count      = 0;
    int lastOffset = 0;

    // Walk the chain to find the last entry
    while ( count < totalSectors ) {
        lastOffset = count;
        DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;
        DBG_ASSERT ( offset > count );
        count      = offset;
        chain++;
    }

    // See if we can append to the last chain
    if ( count > 0 ) {
        int start  = chain[-1].start + (( int ) ( chain[-1].start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain[-1].offset << 4 ) + ( chain[-1].start_offset >> 4 ) + 1;
        if ( index == start + offset - lastOffset ) {
            chain[-1].start_offset = (( totalSectors & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
            chain[-1].offset       = ( totalSectors >> 4 ) & 0xFF;
            totalSectors++;
            FDR->TotalSectors = GetUINT16 ( &totalSectors );
            return true;
        }
    }

    // Start a new chain if there is room
    if ( chain < FDR->DataChain + MAX_CHAINS ) {
        chain->start        = index & 0xFF;
        chain->start_offset = (( totalSectors & 0x0F ) << 4 ) | (( index >> 8 ) & 0x0F );
        chain->offset       = ( totalSectors >> 4 ) & 0xFF;
        totalSectors++;
        FDR->TotalSectors = GetUINT16 ( &totalSectors );
        return true;
    }

    DBG_WARNING ( "Not enough room in CHAIN list" );

    return false;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cDiskFileSystem::FindSector ( int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindSector", true );

    int trackSize = (( m_VIB != NULL ) && ( m_VIB->SectorsPerTrack != 0 )) ? m_VIB->SectorsPerTrack : 9;

    int t = index / trackSize;
    int s = index % trackSize;
    int h = 0;

    if ( t >= m_Media->NumTracks ()) {
        t = 2 * m_Media->NumTracks () - t - 1;
        h = 1;
    }

    if ( t >= m_Media->NumTracks ()) {
        DBG_WARNING ( "Invalid sector index (" << index << ")" );
        return NULL;
    }

    return m_Media->GetSector ( t, h, s );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindSector
// Purpose:     Return a pointer to the requested sector
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sSector *cDiskFileSystem::FindSector ( int index ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindSector", true );

    int trackSize = (( m_VIB != NULL ) && ( m_VIB->SectorsPerTrack != 0 )) ? m_VIB->SectorsPerTrack : 9;

    int t = index / trackSize;
    int s = index % trackSize;
    int h = 0;

    if ( t >= m_Media->NumTracks ()) {
        t = 2 * m_Media->NumTracks () - t - 1;
        h = 1;
    }

    if ( t >= m_Media->NumTracks ()) {
        DBG_WARNING ( "Invalid sector index (" << index << ")" );
        return NULL;
    }

    return m_Media->GetSector ( t, h, s );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FindFileDescriptorIndex
// Purpose:     Return the sector index of the file descriptor with the given filename
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FindFileDescriptorIndex ( const char *name, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FindFileDescriptorIndex", true );

    if ( name != NULL ) {
        UINT16 *dirIndex = ( UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

        int start  = ( dirIndex [0] == 0 ) ? 1 : 0;
        size_t len = ( strlen ( name ) < 10 ) ? strlen ( name ) : 10;

        for ( int i = start; i < 128; i++ ) {
            if ( dirIndex [i] == 0 ) break;
            int index = GetUINT16 ( &dirIndex [i] );
            sFileDescriptorRecord *FDR = ( sFileDescriptorRecord * ) FindSector ( index )->Data;
            if ( strnicmp ( FDR->FileName, name, len ) == 0 ) {
                return index;
            }
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileDescriptorIndex
// Purpose:     Return the sector index of the given FDR
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::GetFileDescriptorIndex ( const sFileDescriptorRecord *FDR, int dir ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetFileDescriptorIndex", true );

    UINT16 *dirIndex = ( UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

    int start  = ( dirIndex [0] == 0 ) ? 1 : 0;

    for ( int i = start; i < 128; i++ ) {
        if ( dirIndex [i] == 0 ) break;
        int index = GetUINT16 ( &dirIndex [i] );
        sFileDescriptorRecord *sector = ( sFileDescriptorRecord * ) FindSector ( index )->Data;
        if ( FDR == sector ) {
            return index;
        }
    }

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::sortDirectoryIndex
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::sortDirectoryIndex ( const void *ptr1, const void *ptr2 )
{
    sFileDescriptorRecord *fdr1 = ( sFileDescriptorRecord * ) sm_SortDisk->FindSector ( GetUINT16 ( ptr1 ))->Data;
    sFileDescriptorRecord *fdr2 = ( sFileDescriptorRecord * ) sm_SortDisk->FindSector ( GetUINT16 ( ptr2 ))->Data;

    return strcmp ( fdr1->FileName, fdr2->FileName );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::AddFileDescriptor ( const sFileDescriptorRecord *FDR, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFileDescriptor", true );

    // Find a place to put the FDR
    int fdrIndex = FindFreeSector ( 0 );
    if ( fdrIndex == -1 ) {
        DBG_WARNING ( "Out of disk space" );
        return -1;
    }

    UINT16 *FDI = ( UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

    sm_SortDisk = this;

    // Preserve the 'visibilty' of files
    int start = (( FDI [0] == 0 ) && ( FDI [1] != 0 )) ? 1 : 0;

    // Look for a free slot in the file descriptor index
    for ( int i = start; i < 127; i++ ) {

        if ( FDI [i] == 0 ) {

            // Mark the new FDR's sector as used
            SetSectorAllocation ( fdrIndex, true );

            // Copy the FDR to the sector on m_Media
            sFileDescriptorRecord *newFDR = ( sFileDescriptorRecord * ) FindSector ( fdrIndex )->Data;
            memcpy ( newFDR, FDR, DEFAULT_SECTOR_SIZE );

            // Make sure the new name is padded with spaces
            for ( size_t j = strlen ( newFDR->FileName ); j < sizeof ( newFDR->FileName ); j++ ) {
                newFDR->FileName [j] = ' ';
            }

            // Zero out the CHAIN list
            newFDR->TotalSectors = 0;
            memset ( newFDR->DataChain, 0, sizeof ( CHAIN ) * MAX_CHAINS );

            // Add the name and resort the directory
            FDI [i] = GetUINT16 ( &fdrIndex );
            qsort ( FDI + start, i - start + 1, sizeof ( UINT16 ), ( QSORT_FUNC ) sortDirectoryIndex );
            break;
        }
    }

    return fdrIndex;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetDirSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::GetDirSector ( int index ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetDirSector", true );

    if ( index == - 1 ) return 1;

    const UINT8 *dir = &m_VIB->reserved2 [ index * 12 ];

    return GetUINT16 ( dir + 10 );
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DirectoryCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::DirectoryCount () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DirectoryCount", true );

    int count = 0;

    while ( count < 3 ) {
        if ( ! IsValidName (( const char * ) &m_VIB->reserved2 [count * 12] )) break;
        count++;
    }

    return count;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DirectoryName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cDiskFileSystem::DirectoryName ( int dir ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DirectoryName", true );

    static char name[12];

    if ( dir == -1 ) return NULL;

    memcpy ( name, m_VIB->reserved2 + dir * 12, 10 );
    name [10] = '\0';

    return name;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FileCount ( int dir ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FileCount", true );

    const UINT16 *dirIndex = ( const UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

    int start = ( dirIndex [0] == 0 ) ? 1 : 0;

    for ( int i = start; i < 128; i++ ) {
        if ( dirIndex [i] == 0 ) return i - start;
    }

    return 127;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cDiskFileSystem::GetFileDescriptor ( int index, int dir ) const
{
    FUNCTION_ENTRY  ( this, "cDiskFileSystem::GetFileDescriptor", false );

    const UINT16 *dirIndex = ( const UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

    int start = ( dirIndex [0] == 0 ) ? 1 : 0;

    const sSector *sector = FindSector ( GetUINT16 ( &dirIndex [start + index] ));

    return ( const sFileDescriptorRecord * ) sector->Data;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::FreeSectors
// Purpose:     Count the number of free sectors on the disk as indicated by the
//              allocation bitmap in the VIB
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::FreeSectors () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::FreeSectors", true );

    int free = 0;

    for ( unsigned i = 0; i < SIZE ( m_VIB->AllocationMap ); i++ ) {
        UINT8 bits = m_VIB->AllocationMap [i];
        for ( int j = 0; j < 8; j++ ) {
            if (( bits & 1 ) == 0 ) free++;
            bits >>= 1;
        }
    }

    return free;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::TotalSectors () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::TotalSectors", true );

    return GetUINT16 ( &m_VIB->FormattedSectors ) - 2;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::VerboseHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cDiskFileSystem::VerboseHeader ( int index ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::VerboseHeader", false );

    static const char *header [2] = {
        " FDI  Chains",
        " ==== ======="
    };

    DBG_ASSERT ( index < 2 );

    return header [index];
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::PrintVerboseInformation
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::PrintVerboseInformation ( FILE *file, const sFileDescriptorRecord *FDR, int dir ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::PrintVerboseInformation", false );

    if ( IsValidFDR ( FDR ) == false ) {
        fprintf ( file, " **** Bad FDR" );
        return;
    }

    fprintf ( file, " %4d", GetFileDescriptorIndex ( FDR, dir ));

    const CHAIN *chain = FDR->DataChain;
    int totalSectors   = GetUINT16 ( &FDR->TotalSectors );

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        DBG_ASSERT ( offset > count );

        fprintf ( file, " %03d/%03d", start, offset - count );

        count = offset;
        chain++;
    }
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetFileSector
// Purpose:     Get then Nth sector for this file.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cDiskFileSystem::GetFileSector ( sFileDescriptorRecord *FDR, int index )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetFileSector", true );

    int totalSectors = GetUINT16 ( &FDR->TotalSectors );

    if ( index >= totalSectors ) {
        DBG_WARNING ( "Requested index (" << index << ") exceeds totalSectors (" << totalSectors << ")" );
        return NULL;
    }

    const CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < totalSectors ) {

        DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        DBG_ASSERT ( offset > count );

        // Is it in this chain?
        if ( index < offset ) {
            int sector = start + ( index - count );
            return FindSector ( sector );
        }

        count = offset;
        chain++;
    }

    DBG_FATAL ( "Internal error: Error traversing file CHAIN" );

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::ExtendFile
// Purpose:     Increase the sector allocation for this file by 'count'.
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cDiskFileSystem::ExtendFile ( sFileDescriptorRecord *FDR, int count )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::ExtendFile", true );

    // Try to extend the file without fragmenting
    int start = FindLastSector ( FDR ) + 1;
    if ( start == 0 ) {
        start = 34;
    }

    for ( int i = 0; i < count; i++ ) {

        int index = FindFreeSector ( start );
        if ( index == -1 ) {
            // Can't stay contiguous, try any available sector - start at sector 34 (from TI-DSR)
            index = FindFreeSector ( 34 );
            if ( index == -1 ) {
                // No 'normal' sectors left - try FDI sector range
                index = FindFreeSector ( 0 );
                if ( index == -1 ) {
                    DBG_WARNING ( "Disk is full" );
                    return i;
                }
            }
        }

        // Add this sector to the file chain and mark it in use
        if ( AddFileSector ( FDR, index ) == false ) {
            DBG_WARNING ( "File is too fragmented" );
            return i;
        }

        sSector *sector = FindSector ( index );
        memset ( sector->Data, 0, DEFAULT_SECTOR_SIZE );

        SetSectorAllocation ( index, true );

        start = index + 1;

        DiskModified ();
    }

    return count;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::TruncateFile
// Purpose:     Free all sectors beyond the indicated sector count limit
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::TruncateFile ( sFileDescriptorRecord *FDR, int limit )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::TruncateFile", true );

    int totalSectors = GetUINT16 ( &FDR->TotalSectors );

    if ( limit >= totalSectors ) {
        DBG_WARNING ( "limit (" << limit << ") exceeds totalSectors (" << totalSectors << ")" );
        return;
    }

    CHAIN *chain = FDR->DataChain;

    // Keep track of how many sectors we've already seen
    int count = 0;

    while ( count < limit ) {

        DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        DBG_ASSERT ( offset > count );

        if ( limit < offset ) {

            // Mark the excess sectors as free
            for ( int i = limit - count; i < offset - count; i++ ) {
                SetSectorAllocation ( start + i, false );
            }

            // Update the chain
            chain->start        = start & 0xFF;
            chain->start_offset = (( limit & 0x0F ) << 4 ) | (( start >> 8 ) & 0x0F );
            chain->offset       = ( limit >> 4 ) & 0xFF;
        }

        count = offset;
        chain++;
    }

    // Zero out the rest of the chain entries & free their sectors
    while ( count < totalSectors ) {

        int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
        int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

        // Mark the sectors as free
        for ( int i = 0; i < offset - count; i++ ) {
            SetSectorAllocation ( start + i, false );
        }

        chain->start        = 0;
        chain->start_offset = 0;
        chain->offset       = 0;

        count = offset;

        chain++;
    }

    FDR->TotalSectors = GetUINT16 ( &limit );

    DiskModified ();
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DiskModified
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cDiskFileSystem::DiskModified ()
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DiskModified", true );

    m_Media->DiskModified ();
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::CheckDisk
// Purpose:     Check the integrity of the data structures on the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::CheckDisk () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::CheckDisk", true );

    if ( m_VIB == NULL ) return false;

    printf ( "Checking disk '%s'\n", m_Media->GetName ());

    int formattedSectors = GetUINT16 ( &m_VIB->FormattedSectors );

    int allocSize = formattedSectors * sizeof ( const sFileDescriptorRecord * );

    const sFileDescriptorRecord **allocationTable = ( const sFileDescriptorRecord ** ) alloca ( allocSize );

    memset ( allocationTable, 0, allocSize );

    bool isOK = true;

    for ( int dir = -1; dir < DirectoryCount (); dir++ ) {

        int fileCount = FileCount ( dir );

        // Create a list of which files are on which sectors
        for ( int i = 0; i < fileCount; i++ ) {

            const sFileDescriptorRecord *FDR = GetFileDescriptor ( i, dir );

            if ( IsValidFDR ( FDR ) == false ) {
                continue;
            }

            int index = GetFileDescriptorIndex ( FDR, dir );
            if ( allocationTable [index] != NULL ) {
                fprintf ( stderr, "File '%10.10s' is cross-linked with file '%10.10s' on sector %d\n", FDR->FileName, allocationTable [index]->FileName, index );
                isOK = false;
            }
            allocationTable [index] = FDR;

            const CHAIN *chain = FDR->DataChain;
            int totalSectors   = GetUINT16 ( &FDR->TotalSectors );

            // Keep track of how many sectors we've already seen
            int count = 0;

            while ( count < totalSectors ) {

                DBG_ASSERT ( chain < FDR->DataChain + MAX_CHAINS );

                int start  = chain->start + (( int ) ( chain->start_offset & 0x0F ) << 8 );
                int offset = (( int ) chain->offset << 4 ) + ( chain->start_offset >> 4 ) + 1;

                DBG_ASSERT ( offset > count );

                for ( int sector = start; sector < start + offset - count; sector++ ) {
                    if ( allocationTable [sector] != NULL ) {
                        fprintf ( stderr, "File '%10.10s' is cross-linked with file '%10.10s' on sector %d\n", FDR->FileName, allocationTable [sector]->FileName, sector );
                        isOK = false;
                    }
                    allocationTable [sector] = FDR;
                }

                count = offset;
                chain++;
            }
        }
    }

    int index = 0;

    for ( unsigned i = 0; i < SIZE ( m_VIB->AllocationMap ); i++ ) {
        UINT8 bits = m_VIB->AllocationMap [i];
        for ( int j = 0; j < 8; j++ ) {
            if (( bits & 1 ) == 0 ) {
                if ( allocationTable [index] != NULL ) {
                    fprintf ( stderr, "Sector %d is marked as free but is used by file '%10.10s'\n", index, allocationTable [index]->FileName );
                    isOK = false;
                }
            }
            bits >>= 1;
            index++;
        }
    }

    return isOK;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetPath
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::GetPath ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetPath", true );

    if ( m_Media == NULL ) return false;

    const char *path = m_Media->GetName ();

    if ( maxLen < strlen ( path ) + 1 ) {
        DBG_ERROR ( "Destination buffer is too short" );
        return false;
    }

    strcpy ( buffer, path );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::GetName
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::GetName ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::GetName", true );

    if ( m_VIB == NULL ) return false;

    size_t length = MAX_FILENAME;

    while (( length > 0 ) && ( m_VIB->VolumeName [ length - 1 ] == ' ' )) {
        length--;
    }

    if ( maxLen < length + 1 ) {
        DBG_ERROR ( "Destination buffer is too short" );
        return false;
    }

    strncpy ( buffer, m_VIB->VolumeName, length );
    buffer [length] = '\0';

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsValid () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::IsValid", true );

    if (( m_Media == NULL ) || ( m_VIB == NULL )) return false;
    if ( m_Media->GetFormat () == FORMAT_UNKNOWN ) return false;
    if ( memcmp ( m_VIB->DSK, "DSK", 3 ) != 0 ) return false;

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::IsCollection
// Purpose:     Return the filename of the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::IsCollection () const
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::IsCollection", true );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cDiskFileSystem::OpenFile ( const char *filename, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::OpenFile", true );

    if ( m_VIB == NULL ) return NULL;

    cFile *file = NULL;

    int index = FindFileDescriptorIndex ( filename, dir );

    if ( index != -1 ) {
        file = cFileSystem::CreateFile (( sFileDescriptorRecord * ) FindSector ( index )->Data );
    }

    return file;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::CreateFile
// Purpose:     Create a new file on the disk (deletes any pre-existing file)
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cDiskFileSystem::CreateFile ( const char *filename, UINT8 type, int recordLength, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::CreateFile", true );

    if ( m_VIB == NULL ) return NULL;

    // Get rid of any existing file by this name
    int index = FindFileDescriptorIndex ( filename, dir );
    if ( index != -1 ) {
        DeleteFile ( filename, dir );
    }

    sFileDescriptorRecord fdr;
    memset ( &fdr, 0, sizeof ( fdr ));

    for ( int i = 0; i < MAX_FILENAME; i++ ) {
        fdr.FileName [i] = ( *filename != '\0' ) ? *filename++ : ' ';
    }

    fdr.FileStatus       = type;
    fdr.RecordsPerSector = ( type & VARIABLE_TYPE ) ? ( 255 / ( recordLength + 1 )) : 256 / recordLength;
    fdr.RecordLength     = recordLength;

    DBG_FATAL ( "Function not implemented yet" );

    return NULL;
}


//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::AddFile ( cFile *file, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::AddFile", true );

    if ( m_VIB == NULL ) return false;

    const sFileDescriptorRecord *FDR = file->GetFDR ();

//    DBG_TRACE ( "Adding file " << file->GetName () << " to disk " << GetName ());

    // Look for a file on this disk with the same name
    int index = FindFileDescriptorIndex ( FDR->FileName, dir );
    if ( index != -1 ) {
        // See if this file we found is the one we're trying to add
        if (( void * ) FDR == ( void * ) FindSector ( index )->Data ) {
            DBG_WARNING ( "File already exists on this disk" );
            return true;
        }

        // Remove the existing file
        DeleteFile ( FDR->FileName, dir );
    }

    // First, make sure the disk has enough room for the file
    int totalSectors = GetUINT16 ( &FDR->TotalSectors );
    if ( FreeSectors () < totalSectors + 1 ) {
        DBG_WARNING ( "Not enough room on disk for file" );
        return false;
    }

    // Next, Try to add another filename
    int fdrIndex = AddFileDescriptor ( FDR, dir );
    if ( fdrIndex == -1 ) {
        DBG_WARNING ( "No room left in the file descriptor table" );
        return false;
    }

    // Get a pointer to the new FDR so we can update the file chain
    sFileDescriptorRecord *newFDR = ( sFileDescriptorRecord * ) FindSector ( fdrIndex )->Data;

    // This shouldn't fail since we've already checked for free space
    if ( ExtendFile ( newFDR, totalSectors ) == false ) {
        DBG_FATAL ( "Internal error: Unable to extend file" );
        DeleteFile ( FDR->FileName, dir );
        return false;
    }

    // Copy the data to the new data sectors
    for ( int i = 0; i < totalSectors; i++ ) {
        sSector *sector = GetFileSector ( newFDR, i );
        file->ReadSector ( i, sector->Data );
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cDiskFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cDiskFileSystem::DeleteFile ( const char *fileName, int dir )
{
    FUNCTION_ENTRY ( this, "cDiskFileSystem::DeleteFile", true );

    if ( m_VIB == NULL ) return false;

    // See if we have a file with this name
    int index = FindFileDescriptorIndex ( fileName, dir );
    if ( index == -1 ) {
        return false;
    }

//    DBG_TRACE ( "Removing file " << fileName << " from disk " << GetName ());

    UINT16 *FDI = ( UINT16 * ) FindSector ( GetDirSector ( dir ))->Data;

    int start = ( FDI [0] == 0 ) ? 1 : 0;

    // Remove the pointer to the FDR from the FDI
    int fdrIndex = GetUINT16 ( &index );
    for ( int i = start; i < 127; i++ ) {
        if ( FDI [i] == fdrIndex ) {
            memcpy ( FDI + i, FDI + i + 1, ( 127 - i ) * sizeof ( UINT16 ));
            FDI [127] = 0;
            break;
        }
    }

    sFileDescriptorRecord *FDR = ( sFileDescriptorRecord * ) FindSector ( index )->Data;

    // Release all of the sectors owned by the file itself
    TruncateFile ( FDR, 0 );

    // Release the FDR's sector
    SetSectorAllocation ( index, false );

    DiskModified ();

    return true;
}
