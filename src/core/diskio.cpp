//----------------------------------------------------------------------------
//
// File:        diskio.cpp
// Date:        14-Jul-2001
// Programmer:  Marc Rousseau
//
// Description: A class to manipulte disk images
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "fileio.hpp"

DBG_REGISTER ( __FILE__ );

static long CF7_DISK_SIZE = 0x0640 * 512;

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

size_t freadcf7 ( void *restrict ptr, size_t size, size_t nitems, FILE *restrict file )
{
    char *buffer = new char [ size * nitems * 2 ];

    // Read the raw sector from the disk
    size_t read = fread ( buffer, 2 * size, nitems, file );
    if ( read == nitems ) {
        for ( size_t i = 0; i < size; i++ ) {
            ((char*)ptr) [i] = buffer [2*i];
        }
    }

    delete [] buffer;

    return read;
}

static size_t fwritecf7 ( void *restrict ptr, size_t size, size_t nitems, FILE *restrict file )
{
    char *buffer = new char [ size * nitems * 2 ];

    memset ( buffer, 0, size * nitems * 2 );

    for ( size_t i = 0; i < size; i++ ) {
        buffer [2*i] = ((char*)ptr) [i];
    }

    size_t written = fwrite ( buffer, 2 * size, nitems, file );

    delete [] buffer;

    return written;
}

cDiskMedia::cDiskMedia ( const char *fileName, int volume ) :
    cBaseObject ( "cDiskMedia" ),
    m_HasChanged ( false ),
    m_IsWriteProtected ( false ),
    m_FileName ( NULL ),
    m_VolumeIndex ( volume ),
    m_Format ( FORMAT_INVALID ),
    m_MaxHeads ( 0 ),
    m_MaxTracks ( 0 ),
    m_NumHeads ( 0 ),
    m_NumTracks ( 0 ),
    m_LastSectorIndex ( 0 ),
    m_RawData ( NULL )
{
    FUNCTION_ENTRY ( this, "cDiskMedia ctor", true );

    memset ( m_Track, 0, sizeof ( m_Track ));

    SetName ( fileName );
    LoadFile ();
}

cDiskMedia::cDiskMedia ( int maxTracks, int maxHeads ) :
    cBaseObject ( "cDiskMedia" ),
    m_HasChanged ( false ),
    m_IsWriteProtected ( false ),
    m_FileName ( NULL ),
    m_VolumeIndex ( 0 ),
    m_Format ( FORMAT_UNKNOWN ),
    m_MaxHeads ( 0 ),
    m_MaxTracks ( 0 ),
    m_NumHeads ( 0 ),
    m_NumTracks ( 0 ),
    m_LastSectorIndex ( 0 ),
    m_RawData ( NULL )
{
    FUNCTION_ENTRY ( this, "cDiskMedia ctor", true );

    memset ( m_Track, 0, sizeof ( m_Track ));

    AllocateTracks ( maxTracks, maxHeads );

    m_HasChanged = false;
}

cDiskMedia::~cDiskMedia ()
{
    FUNCTION_ENTRY ( this, "cDiskMedia dtor", true );

    if ( m_HasChanged == true ) {
        SaveFile ();
    }

    delete [] m_FileName;
    m_FileName = NULL;

    delete [] m_RawData;
    m_RawData = NULL;
}

UINT8 *cDiskMedia::FindAddressMark ( UINT8 mask, UINT8 mark, eDiskDensity density, UINT8 *ptr, const UINT8 *max )
{
    FUNCTION_ENTRY ( NULL, "cDiskMedia::FindAddressMark", false );

restart:

    // Find start of sync bytes
    while ( *ptr != '\x00' ) {
        if ( ++ptr >= max ) return NULL;
    }

    while ( *ptr == '\x00' ) {
        if ( ++ptr >= max ) return NULL;
    }

    if ( density == DENSITY_DOUBLE ) {
        for ( int i = 0; i < 3; i++ ) {
            if ( *ptr != 0xA1 ) goto restart;
            if ( ++ptr >= max ) return NULL;
        }
    }

    if (( *ptr++ & mask ) != mark ) goto restart;

    return ptr;
}

UINT8 *cDiskMedia::FindEndOfTrack ( eDiskDensity density, UINT8, int indexGapSize, UINT8 *start, const UINT8 *max )
{
    FUNCTION_ENTRY ( NULL, "cDiskMedia::FindEndOfTrack", true );

    int eotGAP    = ( density == DENSITY_SINGLE ) ? EOT_FILLER_FM : EOT_FILLER_MFM;
    int syncBytes = ( density == DENSITY_SINGLE ) ? SYNC_BYTES_FM : SYNC_BYTES_MFM;

    eotGAP = 75 * eotGAP / 100;

    UINT8 *ptr = start;

    while ( ptr < max ) {

        UINT8 *markID = FindAddressMark ( 0xFF, 0xFE, density, ptr, max );
        if ( markID == NULL ) return ( ptr == start ) ? NULL : ptr;

        int gapSize = markID - ptr;

        ptr = markID + 6;

        // See if we've passed the EOT filler
        if ( gapSize > eotGAP ) {
            if ( density == DENSITY_DOUBLE ) ptr -= 3;
            ptr -= 1 + syncBytes + indexGapSize + 6;
            return ptr;
        }

        int dataSize = 128 << markID [3];

        // NOTE: WD1771 controllers can use 0xF8 0xF9 0xFA or 0xFB
        // NOTE: PC controllers cannot detect 0xF9 or 0xFA marks
        UINT8 *markData = FindAddressMark ( 0xFC, 0xF8, density, ptr, max );
        if ( markData == NULL ) return NULL;

        ptr = markData + dataSize + 2;
    }

    return NULL;
}

eDiskFormat cDiskMedia::DetermineFormat ( FILE *file )
{
    FUNCTION_ENTRY ( NULL, "cDiskMedia::DetermineFormat", true );

    long fstart = 0;

    // If a volume was specified, make sure it's valid
    if ( m_VolumeIndex > 0 ) {
        // Get the size of the file
        fseek ( file, 0, SEEK_END );
        long fsize = ftell ( file );

        // Make sure the Volume index is valid
        long indexMax = fsize / CF7_DISK_SIZE - 1;
        if (( m_VolumeIndex < 0 ) || ( m_VolumeIndex > indexMax )) {
            DBG_ERROR ( "Invalid volume index " << m_VolumeIndex + 1 << " valid range is 1-" << indexMax + 1 );
            return FORMAT_INVALID;
        }
        fstart = m_VolumeIndex * CF7_DISK_SIZE;
    }

    char testBuffer [64];
    if (( fseek ( file, fstart, SEEK_SET ) != 0 ) ||
        ( fread ( testBuffer, sizeof ( testBuffer ), 1, file ) != 1 ) ||
        ( fseek ( file, fstart, SEEK_SET ) != 0 )) {
        DBG_ERROR ( "Unable to read from file" );
        return FORMAT_INVALID;
    }

    if ( strncmp ( testBuffer + 0x0D, "DSK", 3 ) == 0 ) {
        return FORMAT_RAW_SECTOR;
    }
    if ( strncmp ( testBuffer + 0x15, "DSK", 3 ) == 0 ) {
        return FORMAT_ANADISK;
    }
    if (( testBuffer[0x1A] == 'D' ) && ( testBuffer[0x1C] == 'S' ) && ( testBuffer[0x1E] == 'K' )) {
        return FORMAT_CF7;
    }

    UINT8 indexGapChar   = testBuffer [0];
    eDiskDensity density = ( indexGapChar != 0x4E ) ? DENSITY_SINGLE : DENSITY_DOUBLE;

    // Look for an ID Address Mark
    UINT8 *markID = FindAddressMark ( 0xFF, 0xFE, density, ( UINT8 * ) testBuffer, ( UINT8 * ) testBuffer + 64 );
    if ( markID == NULL ) return FORMAT_UNKNOWN;

    // Look for a valid SYNC byte sequence
    markID -= 1;
    if ( density == DENSITY_DOUBLE ) {
        for ( int i = 1; i <= 3; i++ ) {
            if ( markID [-i] != 0xA1 ) return FORMAT_UNKNOWN;
        }
        markID -= 3;
    }

    for ( int i = 1; i <= 6; i++ ) {
        if ( markID [-i] != 0x00 ) return FORMAT_UNKNOWN;
    }

    return FORMAT_RAW_TRACK;
}

eDiskFormat cDiskMedia::GetFormat () const
{
    FUNCTION_ENTRY ( this, "cDiskMedia::GetFormat", true );

    return m_Format;
}

void cDiskMedia::AllocateTracks ( int maxTracks, int maxHeads )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::AllocateTracks", true );

    DBG_ASSERT (( maxHeads >= 0 ) && ( maxHeads <= 2 ));
    DBG_ASSERT (( maxTracks >= 0 ) && ( maxTracks <= MAX_TRACKS ));

    m_MaxHeads  = ( maxHeads != 0 ) ? maxHeads : 2;
    m_MaxTracks = ( maxTracks != 0 ) ? maxTracks : MAX_TRACKS;

    UINT8 *ptr = new UINT8 [ m_MaxHeads * m_MaxTracks * MAX_TRACK_SIZE ];

    delete [] m_RawData;
    m_RawData = ptr;
    memset ( m_RawData, 0, m_MaxHeads * m_MaxTracks * MAX_TRACK_SIZE );

    memset ( m_Track, 0, sizeof ( m_Track ));

    for ( int h = 0; h < m_MaxHeads; h++ ) {
        for ( int t = 0; t < m_MaxTracks; t++ ) {
            sTrack *track = &m_Track [h][t];
            track->Size = 0;
            track->Data = ptr;
            memset ( track->Sector, 0, sizeof ( track->Sector ));
            ptr += MAX_TRACK_SIZE;
        }
    }

    m_HasChanged = true;
}

void cDiskMedia::FormatTrack ( int tIndex, int hIndex, eDiskDensity density, int count, const sSector *info )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::FormatTrack", false );

    DBG_ASSERT (( tIndex >= 0 ) && ( tIndex < m_MaxTracks ));
    DBG_ASSERT (( hIndex >= 0 ) && ( hIndex < m_MaxHeads ));
    DBG_ASSERT ( density != DENSITY_UNKNOWN );
    DBG_ASSERT (( count > 0 ) && ( count <= MAX_SECTORS ));

    if ( tIndex + 1 > m_NumTracks ) m_NumTracks = tIndex + 1;
    if ( hIndex + 1 > m_NumHeads ) m_NumHeads = hIndex + 1;

    sTrack *track  = &m_Track [hIndex][tIndex];
    track->Density = density;

    sSector *sector = track->Sector;
    memcpy ( sector, info, sizeof ( sSector ) * count );

    UINT8 filler = ( UINT8 ) (( density == DENSITY_SINGLE ) ? 0xFF : 0x4E );
    UINT8 *ptr = track->Data;

    memset ( ptr, filler, MAX_TRACK_SIZE );

    if ( density == DENSITY_SINGLE ) memset ( ptr, 0, INDEX_GAP_FM );

    ptr += ( density == DENSITY_SINGLE ) ? INDEX_GAP_FM : INDEX_GAP_MFM;

    int syncBytes = ( density == DENSITY_SINGLE ) ? SYNC_BYTES_FM : SYNC_BYTES_MFM;
    int idGap     = ( density == DENSITY_SINGLE ) ? ID_DATA_GAP_FM : ID_DATA_GAP_MFM;
    int dataGap   = ( density == DENSITY_SINGLE ) ? DATA_ID_GAP_FM : DATA_ID_GAP_MFM;
    int eotGap    = ( density == DENSITY_SINGLE ) ? EOT_FILLER_FM : EOT_FILLER_MFM;

    for ( int i = 0; i < count; i++ ) {

        memset ( ptr, 0, syncBytes );
        ptr += syncBytes;

        if ( density == DENSITY_DOUBLE ) {
            ptr -= 2;
            memset ( ptr, 0xA1, 3 );
            ptr += 3;
        }

        *ptr++ = 0xFE;                                          // ID Address Mark

        *ptr++ = ( UINT8 ) sector [i].LogicalCylinder;
        *ptr++ = ( UINT8 ) sector [i].LogicalSide;
        *ptr++ = ( UINT8 ) sector [i].LogicalSector;
        *ptr++ = ( UINT8 ) ( sector [i].Size & 0x3F );

        *ptr++ = 0xF7;
        *ptr++ = 0xF7;

        ptr += idGap;

        memset ( ptr, 0, syncBytes );
        ptr += syncBytes;

        if ( density == DENSITY_DOUBLE ) {
            memset ( ptr, 0xA1, 3 );
            ptr += 3;
        }

        *ptr++ = ( UINT8 ) ( 0xFB - (( sector [i].Size & 0xC0 ) >> 6 ));     // Data Address Mark

        sector [i].Data = ptr;

        int size = 128 << ( sector [i].Size & 0x3F );
        memset ( ptr, 0x5E, size );
        ptr += size;

        *ptr++ = 0xF7;
        *ptr++ = 0xF7;

        ptr += dataGap;
    }

    ptr += eotGap - dataGap;

    track->Size = ptr - track->Data;
}

static sSector sectorInfo [] = {
    { 0, 0,  0, 1, NULL },
    { 0, 0,  1, 1, NULL },
    { 0, 0,  2, 1, NULL },
    { 0, 0,  3, 1, NULL },
    { 0, 0,  4, 1, NULL },
    { 0, 0,  5, 1, NULL },
    { 0, 0,  6, 1, NULL },
    { 0, 0,  7, 1, NULL },
    { 0, 0,  8, 1, NULL },
    { 0, 0,  9, 1, NULL },
    { 0, 0, 10, 1, NULL },
    { 0, 0, 11, 1, NULL },
    { 0, 0, 12, 1, NULL },
    { 0, 0, 13, 1, NULL },
    { 0, 0, 14, 1, NULL },
    { 0, 0, 15, 1, NULL },
    { 0, 0, 16, 1, NULL },
    { 0, 0, 17, 1, NULL },
    { 0, 0, 18, 1, NULL },
    { 0, 0, 19, 1, NULL },
    { 0, 0, 20, 1, NULL },
    { 0, 0, 21, 1, NULL },
    { 0, 0, 22, 1, NULL },
    { 0, 0, 23, 1, NULL },
    { 0, 0, 24, 1, NULL },
    { 0, 0, 25, 1, NULL },
    { 0, 0, 26, 1, NULL },
    { 0, 0, 27, 1, NULL },
    { 0, 0, 28, 1, NULL },
    { 0, 0, 29, 1, NULL },
    { 0, 0, 30, 1, NULL },
    { 0, 0, 31, 1, NULL },
    { 0, 0, 32, 1, NULL },
    { 0, 0, 33, 1, NULL },
    { 0, 0, 34, 1, NULL },
    { 0, 0, 35, 1, NULL }
};

//----------------------------------------------------------------------------
//
// Format a 'standard' disk using the given parameters
//
//----------------------------------------------------------------------------

void cDiskMedia::FormatDisk ( int noTracks, int noSides, int noSectors, eDiskDensity density )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ForamtDisk", true );

    AllocateTracks ( noTracks, noSides );

    for ( int h = 0; h < noSides; h++ ) {
        for ( int t = 0; t < noTracks; t++ ) {
            for ( int s = 0; s < noSectors; s++ ) {
                sectorInfo [s].LogicalCylinder = t;
                sectorInfo [s].LogicalSide     = h;
            }
            FormatTrack ( t, h, density, noSectors, sectorInfo );
        }
    }
}

void cDiskMedia::WriteSector ( int tIndex, int hIndex, int logSec, int logCyl, const void *data )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::WriteSector", true );

    sSector *sector = ( sSector * ) GetSector ( tIndex, hIndex, logSec, logCyl );

    if ( sector == NULL ) return;

    if ( memcmp ( sector->Data, data, 128 << sector->Size ) != 0 ) {
        memcpy ( sector->Data, data, 128 << sector->Size );
        m_HasChanged = true;
    }
}

void cDiskMedia::WriteTrack ( int tIndex, int hIndex, size_t size, const UINT8 *data )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::WriteTrack", true );

    DBG_ASSERT (( tIndex >= 0 ) && ( tIndex < m_MaxTracks ));
    DBG_ASSERT (( hIndex >= 0 ) && ( hIndex < m_MaxHeads ));

    if ( tIndex + 1 > m_NumTracks ) m_NumTracks = tIndex + 1;
    if ( hIndex + 1 > m_NumHeads ) m_NumHeads = hIndex + 1;

    sTrack *track = &m_Track [hIndex][tIndex];

    if (( track->Size == size ) && ( memcmp ( track->Data, data, size ) == 0 )) {
        return;
    }

    eDiskDensity density = ( *data == 0x4E ) ? DENSITY_DOUBLE : DENSITY_SINGLE;

    track->Density = density;
    track->Size    = size;

    memcpy ( track->Data, data, size );

    sSector *sector = track->Sector;

    UINT8 *ptr = track->Data;
    UINT8 *max = track->Data + size;

    for ( EVER ) {

        UINT8 *markID = FindAddressMark ( 0xFF, 0xFE, density, ptr, max );
        if ( markID == NULL ) break;

        ptr += 6;

        UINT8 *markData = FindAddressMark ( 0xFC, 0xF8, density, ptr, max );
        if ( markData == NULL ) break;

        sector->LogicalCylinder = markID [0];
        sector->LogicalSide     = markID [1];
        sector->LogicalSector   = markID [2];
        sector->Size            = markID [3];
        sector->Data            = markData;

        ptr = markData + ( 128 << sector->Size ) + 2;

        sector++;
    }

    m_HasChanged = true;
}

//----------------------------------------------------------------------------
//
// Read disk files that contain raw track data.  This is the format used by
// PC99.  This code attempts to correctly read disk images that may contain
// unusual formatting.
//
//----------------------------------------------------------------------------

bool cDiskMedia::ReadDiskRawTrack ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ReadDiskRawTrack", true );

    UINT8 trackBuffer [ 2 * MAX_TRACK_SIZE ];

    // Find out the index GAP character (should be 0xFF or 0x4E but PC99 uses 0x00 for FM disks)
    if ( fread ( trackBuffer, 64, 1, file ) != 1 ) {
        DBG_ERROR ( "Error reading from file" );
        return false;
    }

    UINT8 indexGapChar   = trackBuffer [0];
    eDiskDensity density = ( indexGapChar != 0x4E ) ? DENSITY_SINGLE : DENSITY_DOUBLE;
    size_t maxTrackSize  = 120 * (( density == DENSITY_SINGLE ) ? TRACK_SIZE_FM : TRACK_SIZE_MFM ) / 100;

    UINT8 *markID = FindAddressMark ( 0xFF, 0xFE, density, trackBuffer, trackBuffer + 64 );
    if ( markID == NULL ) return false;
    if ( density == DENSITY_DOUBLE ) markID -= 3;
    markID -= 1 + (( density == DENSITY_SINGLE ) ? SYNC_BYTES_FM : SYNC_BYTES_MFM );
    int indexGapSize = markID - trackBuffer;

    sTrack trackData [ 2 * MAX_TRACKS ];
    memset ( trackData, 0, sizeof ( trackData ));

    size_t count = 0;
    size_t index = 0;

    fseek ( file, 0L, SEEK_SET );

    for ( EVER ) {

        DBG_ASSERT ( count < sizeof ( trackBuffer ));

        size_t read = ( int ) fread ( trackBuffer + count, 1, sizeof ( trackBuffer ) - count, file );

        if (( read == ( size_t ) EOF ) || ( read == 0 )) break;

        count += read;

        UINT8 *trackStart = trackBuffer;

        // trackBuffer should now contain enough data for at least 1 track
        while ( count >= maxTrackSize ) {

            UINT8 *trackEnd = FindEndOfTrack ( density, indexGapChar, indexGapSize, trackStart, trackStart + count );
            if ( trackEnd == NULL ) return false;

            size_t size = trackEnd - trackStart;

            trackData [index].Size = size;
            trackData [index].Data = new UINT8 [ size ];
            memcpy ( trackData [index].Data, trackStart, size );
            index++;

            count      -= size;
            trackStart += size;
        }

        memmove ( trackBuffer, trackStart, count );
    }

    if ( count > 0 ) {
        trackData [index].Size = count;
        trackData [index].Data = new UINT8 [ count ];
        memcpy ( trackData [index].Data, trackBuffer, count );
        index++;
    }

    int maxTrack = MAX_TRACKS_HI;

    // Clear any old data & prepare for a new image
    AllocateTracks ( MAX_TRACKS, 2 );

    // Try to distinguish between single-sided 80 tracks and double-sided 40 track disks
    if ( index == MAX_TRACKS_HI ) {
        WriteTrack ( 0, 0, trackData [MAX_TRACKS_LO].Size, trackData [MAX_TRACKS_LO].Data );
        sSector *sector = &m_Track [0][0].Sector [0];
        if ( sector->LogicalSide == 1 ) maxTrack = MAX_TRACKS_LO;
        m_Track [0][0].Size = 0;
    }

    index = 0;
    for ( int h = 0; h < 2; h++ ) {
        for ( int t = 0; t < MAX_TRACKS; t++ ) {
            if (( t < maxTrack ) && ( trackData [index].Data != NULL )) {
                WriteTrack ( t, h, trackData [index].Size, trackData [index].Data );
                delete [] trackData [index].Data;
                index++;
            }
        }
    }

    DBG_EVENT ( "Disk loaded" );

    return true;
}

//----------------------------------------------------------------------------
//
// Read disk files that contain raw sector data.  This is the format used by
// v9t9.  This code assumes that these files are 'well behaved'.  This means
// that the assumption is made that they are standard disks with no irregular
// properties (i.e. it containes 9/18 256 byte sectors per track).
//
//----------------------------------------------------------------------------

bool cDiskMedia::ReadDiskRawSector ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ReadDiskRawSector", true );

    fseek ( file, 0L, SEEK_END );
    UINT32 size = ftell ( file );
    fseek ( file, 0L, SEEK_SET );

    int totalSectors = size / DEFAULT_SECTOR_SIZE;

    char buffer [ DEFAULT_SECTOR_SIZE ];

    // Read the 1st sector from the disk to get the VIB
    if ( fread ( buffer, sizeof ( buffer ), 1, file ) != 1 ) {
        DBG_ERROR ( "Error reading from file" );
        return false;
    }

    VIB *vib      = ( VIB * ) buffer;
    int noSectors = ( vib->SectorsPerTrack != 0 ) ? vib->SectorsPerTrack : 9;
    int noTracks  = totalSectors / noSectors;

    // Some TI disks don't have accurate # sides & density
    int noSides          = ( noTracks > MAX_TRACKS_LO ) ? 2 : 1;
    eDiskDensity density = ( noSectors == 9 ) ? DENSITY_SINGLE : DENSITY_DOUBLE;

    // Correct the number of tracks
    noTracks /= noSides;

    // Clear any old data & prepare for a new image
    FormatDisk ( noTracks, noSides, noSectors, density );

    fseek ( file, 0L, SEEK_SET );

    for ( int h = 0; h < noSides; h++ ) {
        for ( int t = 0; t < noTracks; t++ ) {
            int track = ( h == 0 ) ? t : noTracks - ( t + 1 );
            for ( int s = 0; s < noSectors; s++ ) {
                if ( fread ( buffer, sizeof ( buffer ), 1, file ) != 1 ) {
                    if ( feof ( file )) {
                        return true;
                    }
                    DBG_ERROR ( "Error reading from file" );
                    return false;
                }
                WriteSector ( track, h, s, track, buffer );
            }
        }
    }

    DBG_EVENT ( "Disk loaded" );

    return true;
}

struct sTrackInfo {
    int       noSectors;
    int       totalSize;
    sSector   sector [18];
};

//----------------------------------------------------------------------------
//
// This routine is designed to read disk files created by AnaDisk that contain
// an eight byte header before each sector.  Disks using this format may have
// irregular sized sectors, 'incorrect' sector numbering schemes, etc.  Using
// this format, it is possible to handle 'copy-protected' disks that utilize
// non-standard disk formatting properties.
//
//----------------------------------------------------------------------------

bool cDiskMedia::ReadDiskAnadisk ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ReadDiskAnadisk", true );

    sTrackInfo info [2][MAX_TRACKS];
    memset ( info, 0, sizeof ( info ));

    // Make a 1st pass to see what the disk looks like
    fseek ( file, 0L, SEEK_SET );

    int noHeads  = -1;
    int noTracks = -1;

    for ( EVER ) {

        sAnadiskHeader header;
        if ( fread ( &header, sizeof ( sAnadiskHeader ), 1, file ) != 1 ) break;

        if (( header.ActualSide >= 2 ) || ( header.ActualCylinder >= MAX_TRACKS )) {
            DBG_WARNING ( "Bad header at offset " << hex << ftell ( file ) - sizeof ( sAnadiskHeader ));
            if (( noHeads == -1 ) || ( noTracks == -1 )) {
                return false;
            }
            break;
        }

        if ( header.ActualSide > noHeads ) noHeads = header.ActualSide;
        if ( header.ActualCylinder > noTracks ) noTracks = header.ActualCylinder;

        // Save the information we need to construct the disk image
        sTrackInfo *track = &info [ header.ActualSide ][ header.ActualCylinder ];

        sSector *sector = &track->sector [track->noSectors];
        sector->LogicalCylinder = header.LogicalCylinder;
        sector->LogicalSide     = header.LogicalSide;
        sector->LogicalSector   = header.LogicalSector;
        sector->Size            = header.Length;

        // Make sure we handle this properly on big-endian machines
        UINT8 *ptr = ( UINT8 * ) &header.DataCount;
        int size = ( ptr [1] << 8 ) | ptr [0];

        if ( size > 0 ) {
            sector->Data = new UINT8 [ size ];
            if ( fread ( sector->Data, size, 1, file ) != 1 ) {
                DBG_ERROR ( "Unable to read sector data" );
                return false;
            }
        }

        track->noSectors++;
        track->totalSize += size;
    }

    // Clear any old data & prepare for a new image
    AllocateTracks ( MAX_TRACKS, 2 );

    // Now, copy the data we read to the proper place on the disk
    for ( int h = 0; h <= noHeads; h++ ) {
        for ( int t = 0; t <= noTracks; t++ ) {
            if ( info [h][t].noSectors == 0 ) continue;
            eDiskDensity density = ( info [h][t].totalSize > TRACK_SIZE_FM ) ? DENSITY_DOUBLE : DENSITY_SINGLE;
            FormatTrack ( t, h, density, info [h][t].noSectors, info [h][t].sector );
            for ( int s = 0; s < info [h][t].noSectors; s++ ) {
                WriteSector ( t, h, info [h][t].sector [s].LogicalSector, info [h][t].sector [s].LogicalCylinder, info [h][t].sector [s].Data );
                delete [] info [h][t].sector [s].Data;
            }
        }
    }

    DBG_EVENT ( "Disk loaded" );

    return true;
}

bool cDiskMedia::ReadDiskCF7 ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ReadDiskCF", true );

    long fstart = m_VolumeIndex * CF7_DISK_SIZE;

    fseek ( file, fstart, SEEK_SET );

    char buffer [ DEFAULT_SECTOR_SIZE ];

    // Read the 1st sector from the disk to get the VIB
    if ( freadcf7 ( buffer, sizeof ( buffer ), 1, file ) != 1 ) {
        DBG_ERROR ( "Error reading from file" );
        return false;
    }

    // Force the disk configuration
    int noTracks           = 40;
    int noSides            = 2;
    int noSectors          = 20;
    eDiskDensity density   = DENSITY_DOUBLE;

    // Clear any old data & prepare for a new image
    FormatDisk ( noTracks, noSides, noSectors, density );

    fseek ( file, fstart, SEEK_SET );

    for ( int h = 0; h < noSides; h++ ) {
        for ( int t = 0; t < noTracks; t++ ) {
            int track = ( h == 0 ) ? t : noTracks - ( t + 1 );
            for ( int s = 0; s < noSectors; s++ ) {
                if ( freadcf7 ( buffer, sizeof ( buffer ), 1, file ) != 1 ) {
                    if ( feof ( file )) {
                        return true;
                    }
                    DBG_ERROR ( "Error reading from file" );
                    return false;
                }
                WriteSector ( track, h, s, track, buffer );
            }
        }
    }

    // Update the disk configuration in the VIB
    sSector *sector = GetSector ( 0, 0, 0 );

    VIB *vib      = ( VIB * ) sector->Data;
    vib->TracksPerSide   = noTracks;
    vib->Sides           = noSides;
    vib->SectorsPerTrack = noSectors;
    vib->Density         = density;

    DBG_EVENT ( "Disk loaded" );

    return true;
}

bool cDiskMedia::LoadFile ()
{
    FUNCTION_ENTRY ( this, "cDiskMedia::LoadFile", true );

    m_Format = FORMAT_INVALID;

    if ( m_FileName == NULL ) {
        DBG_ERROR ( "No filename specified" );
        return false;
    }

    bool retVal = false;
    const char *errMsg = NULL;

    FILE *file = fopen ( m_FileName, "rb" );

    if ( file == NULL ) {
        errMsg = "Unable to open";
    } else {
        errMsg = "Error reading";
        switch ( m_Format = DetermineFormat ( file )) {
            case FORMAT_RAW_TRACK :
                retVal = ReadDiskRawTrack ( file );
                break;
            case FORMAT_RAW_SECTOR :
                retVal = ReadDiskRawSector ( file );
                break;
            case FORMAT_ANADISK :
                retVal = ReadDiskAnadisk ( file );
                break;
            case FORMAT_CF7 :
                retVal = ReadDiskCF7 ( file );
                break;
            default :
                errMsg = "Unable to determine format of";
                break;
        }

        fclose ( file );
    }

    m_IsWriteProtected = false;

    if ( retVal != true ) {
        DBG_WARNING ( errMsg << " disk image '" << m_FileName << '\'' );
        if ( m_Format != FORMAT_INVALID ) {
            AllocateTracks ( m_MaxTracks, m_MaxHeads );
            m_Format = FORMAT_UNKNOWN;
        }
    } else {
        m_IsWriteProtected = IsWriteable ( m_FileName ) ? false : true;
    }

    m_HasChanged = false;

    return retVal;
}

bool cDiskMedia::IsValidRawSector ()
{
    FUNCTION_ENTRY ( this, "cDiskMedia::IsValidRawSector", true );

    // Look for features that will get lost/destroyed if saved in raw sector format
    for ( int h = 0; h < m_NumHeads; h++ ) {
        for ( int t = 0; t < m_NumTracks; t++ ) {
            int track = ( h == 0 ) ? t : m_NumTracks - ( t + 1 );
            const sTrack *ptr = GetTrack ( track, h );
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                const sSector *sector = GetSector ( track, h, s );
                if ( sector == NULL ) {
                    if (( s == 0 ) && ( ptr->Sector [0].Data != NULL )) {
                        return false;
                    }
                    continue;
                }
                if (( sector->Size != 1 ) || ( sector->Data [-1] != 0xFB )) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool cDiskMedia::SaveDiskRawTrack ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SaveDiskRawTrack", true );

    for ( int h = 0; h < m_NumHeads; h++ ) {
        for ( int t = 0; t < m_NumTracks; t++ ) {
            sTrack *track = &m_Track [h][t];
            if ( track->Size != 0 ) {
                if ( fwrite ( track->Data, track->Size, 1, file ) != 1 ) {
                    DBG_ERROR ( "Error writing to file" );
                    return false;
                }
            }
        }
    }

    return true;
}

bool cDiskMedia::SaveDiskRawSector ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SaveDiskRawSector", true );

    for ( int h = 0; h < m_NumHeads; h++ ) {
        for ( int t = 0; t < m_NumTracks; t++ ) {
            int track = ( h == 0 ) ? t : m_NumTracks - ( t + 1 );
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                const sSector *sector = GetSector ( track, h, s );
                if ( sector == NULL ) continue;
                if ( sector->Data == NULL ) break;
                if ( fwrite ( sector->Data, 128 << sector->Size, 1, file ) != 1 ) {
                    DBG_ERROR ( "Error writing to file" );
                    return false;
                }
            }
        }
    }

    return true;
}

bool cDiskMedia::SaveDiskAnadisk ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SaveDiskAnadisk", true );

    for ( int h = 0; h < m_NumHeads; h++ ) {
        for ( int t = 0; t < m_NumTracks; t++ ) {
            sSector *sector = m_Track [h][t].Sector;
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                if ( sector [s].Data == NULL ) break;

                sAnadiskHeader header;
                header.ActualCylinder  = ( UINT8 ) t;
                header.ActualSide      = ( UINT8 ) h;
                header.LogicalCylinder = ( UINT8 ) sector [s].LogicalCylinder;
                header.LogicalSide     = ( UINT8 ) sector [s].LogicalSide;
                header.LogicalSector   = ( UINT8 ) sector [s].LogicalSector;
                header.Length          = ( UINT8 ) sector [s].Size;
                header.DataCount       = ( UINT16 ) ( 128 << sector [s].Size );

                // Encode any non-standard Data Address Marks
                header.Length |= ( UINT8 ) (( 0xFB - sector->Data [-1] ) << 6 );

                if (( fwrite ( &header, sizeof ( header ), 1, file ) != 1 ) ||
                    ( fwrite ( sector [s].Data, 128 << sector [s].Size, 1, file ) != 1 )) {
                    DBG_ERROR ( "Error writing to file" );
                    return false;
                }
            }
        }
    }

    return true;
}

bool cDiskMedia::SaveDiskCF7 ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SaveDiskCF7", true );

    fseek ( file, m_VolumeIndex * CF7_DISK_SIZE, SEEK_SET );

    // TODO - Restore the disk configuration values in the VIB

    for ( int h = 0; h < m_NumHeads; h++ ) {
        for ( int t = 0; t < m_NumTracks; t++ ) {
            int track = ( h == 0 ) ? t : m_NumTracks - ( t + 1 );
            for ( int s = 0; s < MAX_SECTORS; s++ ) {
                const sSector *sector = GetSector ( track, h, s );
                if ( sector == NULL ) continue;
                if ( sector->Data == NULL ) break;
                if ( fwritecf7 ( sector->Data, DEFAULT_SECTOR_SIZE , 1, file ) != 1 ) {
                    DBG_ERROR ( "Error writing to file" );
                    return false;
                }
            }
        }
    }

    return true;
}

void cDiskMedia::ClearDisk ()
{
    FUNCTION_ENTRY ( this, "cDiskMedia::ClearDisk", true );

    AllocateTracks ( m_MaxTracks, m_MaxHeads );

    m_HasChanged = false;
}

bool cDiskMedia::LoadFile ( const char *name )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::LoadFile", true );

    SetName ( name );

    return LoadFile ();
}

bool cDiskMedia::SaveFile ( eDiskFormat format, bool force )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SaveFile", true );

    if ( m_FileName == NULL ) {
        DBG_ERROR ( "No filename specified" );
        return false;
    }

    if ( format == FORMAT_INVALID ) {
        DBG_ERROR ( "Invalid media file" );
        return false;
    }

    if (( force == false ) && ( m_HasChanged == false )) {
        return true;
    }

    if ( format == FORMAT_UNKNOWN ) {
        format = m_Format;
        if ( format == FORMAT_UNKNOWN ) {
            format = FORMAT_RAW_TRACK;
        }
    }

    if (( format == FORMAT_RAW_SECTOR ) && ( IsValidRawSector () == false )) {
        DBG_ERROR ( "Disk contains features that this format does not support" );
        return false;
    }

    if (( m_Format == FORMAT_CF7 ) && ( format != FORMAT_CF7 )) {
        DBG_ERROR ( "Rewriting CF7+ disks is not supported" );
        return false;
    }

    const char *mode = ( format != FORMAT_CF7 ) ? "wb" : "rb+";
    FILE *file = fopen ( m_FileName, mode );
    if ( file == NULL ) {
        DBG_ERROR ( "Unable to open file " << m_FileName << " for writing (mode '" << mode << "') - errno: " << errno );
        return false;
    }

    bool retVal = false;
    const char *errMsg = NULL;

    switch ( format ) {
        case FORMAT_RAW_TRACK :
            retVal = SaveDiskRawTrack ( file );
            break;
        case FORMAT_RAW_SECTOR :
            retVal = SaveDiskRawSector ( file );
            break;
        case FORMAT_ANADISK :
            retVal = SaveDiskAnadisk ( file );
            break;
        case FORMAT_CF7 :
            retVal = SaveDiskCF7 ( file );
            break;
        default :
            errMsg = "Invalid format for";
            break;
    }

    fclose ( file );

    if ( errMsg != NULL ) {
        DBG_WARNING ( errMsg << " disk file '" << m_FileName << '\'' );
    }

    if ( retVal == true ) {
        m_HasChanged = false;
        m_Format     = format;
    }

    return retVal;
}

void cDiskMedia::SetName ( const char *fileName )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::SetName", true );

    delete [] m_FileName;
    m_FileName = NULL;

    if ( fileName != NULL ) {
        size_t len = strlen ( fileName ) + 1;
        m_FileName = new char [ len ];
        strcpy ( m_FileName, fileName );
    }

    m_HasChanged = true;
}

sTrack *cDiskMedia::GetTrack ( int tIndex, int hIndex )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::GetTrack", true );

    if (( tIndex < 0 ) || ( tIndex >= m_MaxTracks )) return NULL;
    if (( hIndex < 0 ) || ( hIndex >= m_MaxHeads )) return NULL;

    return &m_Track [hIndex][tIndex];
}

sSector *cDiskMedia::GetSector ( int tIndex, int hIndex, int sIndex, int track )
{
    FUNCTION_ENTRY ( this, "cDiskMedia::GetSector", true );

    if (( tIndex < 0 ) || ( tIndex >= m_MaxTracks )) return NULL;
    if (( hIndex < 0 ) || ( hIndex >= m_MaxHeads )) return NULL;

    if ( track == -1 ) track = tIndex;

    for ( int i = 0; i < MAX_SECTORS; i++ ) {
        int index = ( m_LastSectorIndex + i + 1 ) % MAX_SECTORS;
        sSector *sector = &m_Track [hIndex][tIndex].Sector [index];
        if ( sector->Data == NULL ) continue;
        if (( sector->LogicalSector == sIndex ) &&
            ( sector->LogicalCylinder == track )) {
            m_LastSectorIndex = index;
            return sector;
        }
    }

    DBG_WARNING ( "Sector " << sIndex << " (t:" << tIndex << ",h:" << hIndex << ") not found on track " << track );

    return NULL;
}
