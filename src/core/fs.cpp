//----------------------------------------------------------------------------
//
// File:        fs.cpp
// Date:        05-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A base class for TI filesystem classes
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "fs.hpp"
#include "diskio.hpp"
#include "diskfs.hpp"
#include "arcfs.hpp"
#include "pseudofs.hpp"
#include "fileio.hpp"

DBG_REGISTER ( __FILE__ );

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
// Procedure:   cFileSystem::Open
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFileSystem *cFileSystem::Open ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::Open", true );

    cFileSystem *fallback = NULL;
    cFileSystem *disk = cDiskFileSystem::Open ( filename, path );
    if (( disk == NULL ) || ( disk->IsValid () == false )) {
        fallback = disk;
        disk = cArchiveFileSystem::Open ( filename, path );
        if (( disk == NULL ) || ( disk->IsValid () == false )) {
            if ( fallback == NULL ) fallback = disk;
            else if ( disk != NULL ) disk->Release( NULL );
            disk = cPseudoFileSystem::Open ( filename, path );
        }
    }

    if ( disk == NULL ) return fallback;

    if ( fallback != NULL ) fallback->Release( NULL );

    return disk;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidName ( const char *name )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::IsValidName", true );

    int i = 0;

    for ( ; i < MAX_FILENAME; i++ ) {
        if ( name [i] == '.' ) return false;
        if ( name [i] == ' ' ) break;
        if ( ! isprint ( name [i] & 0xFF )) return false;
    }

    for ( i++; i < MAX_FILENAME; i++ ) {
        if ( name [i] != ' ' ) return false;
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::IsValidFDR
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::IsValidFDR ( const sFileDescriptorRecord *fdr )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::IsValidFDR", true );

    int totalSectors = GetUINT16 ( &fdr->TotalSectors );

    // Make sure the filename valid
    if ( IsValidName ( fdr->FileName ) == false ) return false;

    // Simple sanity checks
    if (( fdr->FileStatus & PROGRAM_TYPE ) != 0 ) {
        if (( fdr->FileStatus & ( INTERNAL_TYPE | VARIABLE_TYPE )) != 0 ) return false;
        if ( fdr->RecordsPerSector != 0 ) return false;
    } else {
        if ( fdr->RecordsPerSector * fdr->RecordLength > DEFAULT_SECTOR_SIZE ) return false;
        int recordCount = GetUINT16_LE ( &fdr->NoFixedRecords );
        if (( fdr->FileStatus & VARIABLE_TYPE ) == 0 ) {
            if ( fdr->EOF_Offset != 0 ) return false;
        } else {
            if ( recordCount != totalSectors ) return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::CreateFile
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
cFile *cFileSystem::CreateFile ( sFileDescriptorRecord *fdr )
{
    FUNCTION_ENTRY ( this, "cFileSystem::CreateFile", true );

    return new cFile ( this, fdr );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::VerboseHeader
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cFileSystem::VerboseHeader ( int ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::VerboseHeader", false );

    return "";
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::PrintVerboseInformation
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::PrintVerboseInformation ( FILE *, const sFileDescriptorRecord *, int ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::PrintVerboseInformation", false );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::CheckDisk
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
bool cFileSystem::CheckDisk () const
{
    FUNCTION_ENTRY ( this, "cFileSystem::CheckDisk", true );

    return true;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::DirectoryCount
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cFileSystem::DirectoryCount () const
{
    FUNCTION_ENTRY ( this, "cFileSystem::DirectoryCount", true );

    return 0;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::DirectoryName
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
const char *cFileSystem::DirectoryName ( int ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::DirectoryName", true );

    return NULL;
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::PrintTimestamp
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void  cFileSystem::PrintTimestamp ( FILE *file, const UINT8 *ptr )
{
    FUNCTION_ENTRY ( NULL, "cFileSystem::PrintTimestamp", true );

    if ( * ( const UINT32 * ) ptr != 0 ) {
        int year    = ( ptr [2] >> 1 ) & 0x7F;
        int month   = (( ptr [2] << 3 ) | ( ptr [3] >> 5 )) & 0x0F;
        int day     = ( ptr [3] ) & 0x1F;

        fprintf ( file, " %2d/%02d/%4d", month, day, year + (( year < 80 ) ? 2000 : 1900 ));

        int hour    = ( ptr [0] >> 3 ) & 0x1F;
        int minutes = (( ptr [0] << 3 ) | ( ptr [1] >> 5 )) & 0x3F;
        int seconds = ( ptr [1] ) & 0x1F;

        fprintf ( file, " %02d:%02d:%02d", hour, minutes, seconds );
    } else {
        fprintf ( file, "                    " );
    }
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::ShowDirectory
// Purpose:
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
void cFileSystem::ShowDirectory ( FILE *file, bool verbose ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::ShowDirectory", true );

    if ( IsValid () == false ) {
        fprintf ( file, "Unrecognized media format\n" );
        return;
    }

    char name [MAX_FILENAME+1];
    GetName ( name, sizeof ( name ));

    bool bFoundTimestamp = false;

    for ( int dir = -1; dir < DirectoryCount (); dir++ ) {

        int fileCount = FileCount ( dir );

        for ( int i = 0; i < fileCount; i++ ) {
            const sFileDescriptorRecord *FDR = GetFileDescriptor ( i, dir );
            for ( int j = 0; j < 8; j++ ) {
                if ( FDR->reserved2 [j] != 0 ) {
                    bFoundTimestamp = true;
                    break;
                }
            }

            if ( bFoundTimestamp == true ) break;
        }
    }

    int sectorsUsed = 0;

    for ( int dir = -1; dir < DirectoryCount (); dir++ ) {

        const char *dirName = DirectoryName ( dir );

        fprintf ( file, "\n" );
        fprintf ( file, "Directory of %s%s%s\n", name, dirName ? "." : "", dirName ? dirName : "" );
        fprintf ( file, "\n" );
        fprintf ( file, "  Filename   Size    Type     P%s%s\n", ( bFoundTimestamp == true ) ? " Created             Modified           " : "", ( verbose == true ) ? VerboseHeader ( 0 ) : "" );
        fprintf ( file, " ==========  ==== =========== =%s%s\n", ( bFoundTimestamp == true ) ? " ========== ======== ========== ========" : "", ( verbose == true ) ? VerboseHeader ( 1 ) : "" );

        int fileCount = FileCount ( dir );

        for ( int i = 0; i < fileCount; i++ ) {

            const sFileDescriptorRecord *FDR = GetFileDescriptor ( i, dir );

            fprintf ( file, "  %10.10s", FDR->FileName );
            int size = GetUINT16 ( &FDR->TotalSectors ) + 1;
            sectorsUsed += size;
            fprintf ( file, " %4d", size );
            if ( FDR->FileStatus & PROGRAM_TYPE ) {
                fprintf ( file, " PROGRAM    " );
            } else {
                fprintf ( file, " %s/%s %3d", ( FDR->FileStatus & INTERNAL_TYPE ) ? "INT" : "DIS",
                                       ( FDR->FileStatus & VARIABLE_TYPE ) ? "VAR" : "FIX",
                                       FDR->RecordLength ? FDR->RecordLength : DEFAULT_SECTOR_SIZE );
            }

            printf (( FDR->FileStatus & WRITE_PROTECTED_TYPE ) ? " Y" : "  " );

            if ( bFoundTimestamp == true ) {
                PrintTimestamp ( file, FDR->reserved2 );
                PrintTimestamp ( file, FDR->reserved2 + 4 );
            }

            if ( verbose == true ) {
                PrintVerboseInformation ( file, FDR, dir );
            }

            fprintf ( file, "\n" );
        }
    }

    int totalSectors   = TotalSectors ();
    int totalAvailable = FreeSectors ();

    fprintf ( file, "\n" );
    fprintf ( file, "  Available: %4d  Used: %4d\n", totalAvailable, sectorsUsed );
    fprintf ( file, "      Total: %4d   Bad: %4d\n", totalSectors, totalSectors - sectorsUsed - totalAvailable );
    fprintf ( file, "\n" );
}

//------------------------------------------------------------------------------
// Procedure:   cFileSystem::GetFilenames
// Purpose:     Return a list of all files on the disk
// Parameters:
// Returns:
// Notes:
//------------------------------------------------------------------------------
int cFileSystem::GetFilenames ( char *names[], int dir ) const
{
    FUNCTION_ENTRY ( this, "cFileSystem::GetFilenames", true );

    int fileCount = FileCount ( dir );

    for ( int i = 0; i < fileCount; i++ ) {

        const sFileDescriptorRecord *FDR = GetFileDescriptor ( i, dir );

        names [i] = new char [11];
        memcpy ( names [i], FDR->FileName, 10 );
        names [i][10] = '\0';
        for ( int j = 9; j > 0; j-- ) {
            if ( names [i][j] != ' ' ) break;
            names [i][j] = '\0';
        }

        DBG_TRACE ( names [i] );
    }

    return fileCount;
}
