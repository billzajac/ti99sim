//----------------------------------------------------------------------------
//
// File:        arcfs.cpp
// Date:        15-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to simulate a filesystem for an ARC file (Barry Boone's Archive format)
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

#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "diskio.hpp"
#include "arcfs.hpp"
#include "pseudofs.hpp"
#include "fileio.hpp"
#include "decodelzw.hpp"

DBG_REGISTER ( __FILE__ );

// 18-byte structure used to hold file descriptors in .ark files

struct sArcFileDescriptorRecord {
    char                FileName [ MAX_FILENAME ];
    UINT8               FileStatus;
    UINT8               RecordsPerSector;
    UINT16              TotalSectors;
    UINT8               EOF_Offset;
    UINT8               RecordLength;
    UINT16              NoFixedRecords;               // For some strange reason, this is little-endian!
};

struct sDecodeInfo {
    cArchiveFileSystem *FileSystem;
    cDecodeLZW         *pDecoder;
    int                 FileIndex;
};

static inline UINT16 GetUINT16 ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [0] << 8 ) | ptr [1] );
}

static inline UINT16 GetUINT16_LE ( const void *_ptr )
{
    FUNCTION_ENTRY ( NULL, "GetUINT16_LE", true );

    const UINT8 *ptr = ( const UINT8 * ) _ptr;
    return ( UINT16 ) (( ptr [1] << 8 ) | ptr [0] );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem *cArchiveFileSystem::Open ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "cArchiveFileSystem::Open", true );

    cPseudoFileSystem *container = cPseudoFileSystem::Open ( filename, path );

    if (( container != NULL ) && ( container->IsValid ())) {
        cArchiveFileSystem *disk = new cArchiveFileSystem ( container );
        container->Release ( NULL );
        return disk;
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::cArchiveFileSystem ( cPseudoFileSystem *container ) :
    cFileSystem ( "cArchiveFileSystem" ),
    m_Container ( container ),
    m_FileCount ( 0 ),
    m_TotalSectors ( 0 )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem ctor", true );

    memset ( m_Directory, 0, sizeof ( m_Directory ));

    m_Container->AddRef ( this );

    LoadFile ();
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::~cArchiveFileSystem
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cArchiveFileSystem::~cArchiveFileSystem ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem dtor", true );

    for ( int i = 0; i < m_FileCount; i++ ) {
        delete m_Directory [i].sector;
        delete [] m_Directory [i].fileData;
    }

    m_Container->Release ( this );
    m_Container = NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DirectoryCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DirectoryCallback ( void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( token, "cArchiveFileSystem::DirectoryCallback", false );

    sDecodeInfo *pData = ( sDecodeInfo * ) token;

    cArchiveFileSystem *me = ( cArchiveFileSystem * ) pData->FileSystem;

    sArcFileDescriptorRecord *arc = ( sArcFileDescriptorRecord * ) buffer;

    // Populate the directory
    for ( int i = 0; i < 14; i++ ) {

        if ( IsValidName ( arc->FileName ) == false ) break;

        sFileDescriptorRecord *fdr = &me->m_Directory [me->m_FileCount++].fdr;

        // Copy the fields from the .ark directory to the FDR
        memcpy ( fdr->FileName, arc->FileName, MAX_FILENAME );
        fdr->FileStatus        = arc->FileStatus;
        fdr->RecordsPerSector  = arc->RecordsPerSector;
        fdr->TotalSectors      = arc->TotalSectors;
        fdr->EOF_Offset        = arc->EOF_Offset;
        fdr->RecordLength      = arc->RecordLength;
        fdr->NoFixedRecords    = arc->NoFixedRecords;

        int totalSectors = GetUINT16 ( &fdr->TotalSectors );

        me->m_TotalSectors += totalSectors + 1;

        arc++;
    }

    // Look for the end of the directory and swith to the data callback
    if ( memcmp (( char * ) buffer + 252, "END!", 4 ) == 0 ) {
        // Set up the call back for the first file
        return DataCallback ( buffer, size, token );
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DataCallback
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DataCallback ( void *buffer, size_t size, void *token )
{
    FUNCTION_ENTRY ( token, "cArchiveFileSystem::DataCallback", false );

    sDecodeInfo *pData = ( sDecodeInfo * ) token;

    cArchiveFileSystem *me = ( cArchiveFileSystem * ) pData->FileSystem;

    // Get the FDR for the next file in line
    sFileInfo *info = &me->m_Directory [pData->FileIndex++];

    unsigned int bytesLeft = pData->pDecoder->BytesLeft ();

    if ( pData->FileIndex > 1 ) {
        // Update bytesUsed to reflect the actual number used by the last file
        info [-1].bytesUsed -= bytesLeft;

        // Allocate a fake sector for use by this file
        sSector *sector = new sSector;
        memset ( sector, 0, sizeof ( sSector ));
        sector->Size = DEFAULT_SECTOR_SIZE;
        * ( sSector ** ) &info [-1].sector = sector;
    }

    // There's nothing more to do for the last call
    if ( pData->FileIndex == me->m_FileCount + 1 ) return true;

    int totalSectors = GetUINT16 ( &info->fdr.TotalSectors );

    // Allocate space for the file's data and store the pointer in a reserved portion of the FDR
    size   = DEFAULT_SECTOR_SIZE * totalSectors;
    buffer = new UINT8 [ DEFAULT_SECTOR_SIZE * totalSectors ];

    info->fileData = ( UINT8 * ) buffer;
    info->bytesUsed = bytesLeft;

    pData->pDecoder->SetWriteCallback ( DataCallback, buffer, size, token );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::LoadFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::LoadFile ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::LoadFile", true );

    cFile *file = m_Container->OpenFile ( NULL, -1 );
    if ( file == NULL ) {
        return;
    }

    sFileDescriptorRecord *fdr = file->GetFDR ();

    int recLen   = fdr->RecordLength;
    int recCount = GetUINT16_LE ( &fdr->NoFixedRecords );
    int size     = recLen * recCount;

    UINT8 *inputBuffer = new UINT8 [ size ];
    for ( int i = 0; i < recCount; i++ ) {
        file->ReadRecord ( inputBuffer + (( long ) i * recLen ), recLen );
    }

    // If it looks like it might be an archive, decode it
    if ( inputBuffer [0] == 0x80 ) {
        cDecodeLZW *pDecoder = new cDecodeLZW ( 12 );
        char buffer [256];
        sDecodeInfo data = { this, pDecoder, 0 };

        pDecoder->SetWriteCallback ( DirectoryCallback, buffer, 256, &data );
        pDecoder->ParseBuffer ( inputBuffer, size );
        delete pDecoder;

        DBG_ASSERT ( data.FileIndex > 0 );

        // If there was a problem, zero out the size of the affected files
        for ( int i = data.FileIndex - 1; i < m_FileCount; i++ ) {
            m_Directory [i].fdr.TotalSectors = 0;
        }
    }

    delete [] inputBuffer;

    file->Release ( NULL );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileInfo
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const cArchiveFileSystem::sFileInfo *cArchiveFileSystem::GetFileInfo ( const sFileDescriptorRecord *fdr ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFileInfo", true );

    for ( size_t i = 0; i < SIZE ( m_Directory ); i++ ) {
        if ( fdr == &m_Directory [i].fdr ) {
            return &m_Directory [i];
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::FileCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FileCount ( int ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::FileCount", true );

    return m_FileCount;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileDescriptor
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const sFileDescriptorRecord *cArchiveFileSystem::GetFileDescriptor ( int index, int ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFileDescriptor", true );

    DBG_ASSERT ( index < m_FileCount );

    return &m_Directory [index].fdr;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFreeSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::FreeSectors () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFreeSectors", true );

    return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetTotalSectors
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::TotalSectors () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetTotalSectors", true );

    return m_TotalSectors;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::VerboseHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cArchiveFileSystem::VerboseHeader ( int index ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::VerboseHeader", false );

    static const char *header [2] = {
        " Ratio",
        " ====="
    };

    return header [ index ];
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::PrintVerboseInformation
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::PrintVerboseInformation ( FILE *file, const sFileDescriptorRecord *fdr, int ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::PrintVerboseInformation", false );

    const sFileInfo *info = GetFileInfo ( fdr );

    // If we allocated an sSector for this file, it's valid
    bool bad = ( info->sector == NULL ) ? true : false;

    UINT16 totalBytes = GetUINT16 ( &fdr->TotalSectors ) * DEFAULT_SECTOR_SIZE;

    float ratio = 100.0 * info->bytesUsed / totalBytes;

    fprintf ( file, bad ? " *BAD*" : "%5.1f%% - %d/%d", ratio, info->bytesUsed, totalBytes );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetFileSector
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
sSector *cArchiveFileSystem::GetFileSector ( sFileDescriptorRecord *fdr, int index )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetFileSector", true );

    const sFileInfo *info = GetFileInfo ( fdr );

    sSector *sector = info->sector;

    UINT8 *fileBuffer = info->fileData;

    sector->Data = fileBuffer + (( long ) index * DEFAULT_SECTOR_SIZE );

    return sector;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::ExtendFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cArchiveFileSystem::ExtendFile ( sFileDescriptorRecord *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::ExtendFile", true );

    DBG_FATAL ( "Function not implemented" );

    return -1;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::TruncateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::TruncateFile ( sFileDescriptorRecord *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::TruncateFile", true );

    DBG_FATAL ( "Function not implemented" );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DiskModified
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cArchiveFileSystem::DiskModified ()
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::DiskModified", true );

    DBG_FATAL ( "Function not implemented" );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetPath
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::GetPath ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetPath", true );

    return m_Container->GetPath ( buffer, maxLen );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::GetName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::GetName ( char *buffer, size_t maxLen ) const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::GetName", true );

    return m_Container->GetName ( buffer, maxLen );
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsValid
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsValid () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::IsValid", true );

    return ( m_FileCount > 0 ) ? true : false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::IsCollection
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::IsCollection () const
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::IsCollection", true );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::OpenFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cArchiveFileSystem::OpenFile ( const char *filename, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::OpenFile", true );

    size_t len = strlen ( filename );

    for ( int i = 0; i < m_FileCount; i++ ) {
        sFileDescriptorRecord *fdr = &m_Directory [i].fdr;
        if ( strnicmp ( fdr->FileName, filename, len ) == 0 ) {
            // Watch out for bad/corrupt files
            return ( fdr->TotalSectors == 0 ) ? NULL : cFileSystem::CreateFile ( fdr );
        }
    }

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cArchiveFileSystem::CreateFile ( const char *, UINT8, int, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::CreateFile", true );

    DBG_FATAL ( "Function not supported" );

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::AddFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::AddFile ( cFile *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::AddFile", true );

    DBG_FATAL ( "Function not implemented" );

    return false;
}

//------------------------------------------------------------------------------
// Procedure:   cArchiveFileSystem::DeleteFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cArchiveFileSystem::DeleteFile ( const char *, int )
{
    FUNCTION_ENTRY ( this, "cArchiveFileSystem::DeleteFile", true );

    DBG_FATAL ( "Function not implemented" );

    return false;
}
