//----------------------------------------------------------------------------
//
// File:        fs.hpp
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

#ifndef FS_HPP_
#define FS_HPP_

#include "cBaseObject.hpp"

// Flags used by TI to indicate file types

#define DATA_TYPE               0x00
#define PROGRAM_TYPE            0x01

#define DISPLAY_TYPE            0x00
#define INTERNAL_TYPE           0x02

#define WRITE_PROTECTED_TYPE    0x08

#define FIXED_TYPE              0x00
#define VARIABLE_TYPE           0x80

const int MAX_FILENAME          = 10;
const int MAX_CHAINS            = 76;
const int MAX_FILES             = 127;      // Maximum number of files that can fit on a disk (DEFAULT_SECTOR_SIZE/2-1)

// On-Disk structures used by TI

struct VIB {
    char      VolumeName [ MAX_FILENAME ];
    UINT16    FormattedSectors;
    UINT8     SectorsPerTrack;
    char      DSK [ 3 ];
    UINT8     reserved;
    UINT8     TracksPerSide;
    UINT8     Sides;
    UINT8     Density;
    UINT8     reserved2 [ 36 ];
    UINT8     AllocationMap [ 200 ];
};

struct CHAIN {
    UINT8     start;
    UINT8     start_offset;
    UINT8     offset;
};

struct sFileDescriptorRecord {
    char      FileName [ MAX_FILENAME ];
    UINT8     reserved1 [ 2 ];
    UINT8     FileStatus;
    UINT8     RecordsPerSector;
    UINT16    TotalSectors;
    UINT8     EOF_Offset;
    UINT8     RecordLength;
    UINT16    NoFixedRecords;               // For some strange reason, this is little-endian!
    UINT8     reserved2 [ 8 ];
    CHAIN     DataChain [ MAX_CHAINS ];
};

struct sSector;

class cFile;

class cFileSystem : public cBaseObject {

private:

    // Disable the copy constructor and assignment operator defaults
    cFileSystem ( const cFileSystem & );			// no implementation
    void operator = ( const cFileSystem & );		// no implementation

protected:

    cFileSystem ( const char *name ) : cBaseObject ( name ) {}
    ~cFileSystem () {}

    cFile *CreateFile ( sFileDescriptorRecord *FDR );

    // Functions used by ShowDirectory
    static void PrintTimestamp ( FILE *file, const UINT8 *ptr );

    virtual int DirectoryCount () const;
    virtual const char *DirectoryName ( int ) const;
    virtual int FileCount ( int ) const = 0;
    virtual const sFileDescriptorRecord * GetFileDescriptor ( int, int ) const = 0;
    virtual int FreeSectors () const = 0;
    virtual int TotalSectors () const = 0;
    virtual const char *VerboseHeader ( int ) const;
    virtual void PrintVerboseInformation ( FILE *, const sFileDescriptorRecord *, int ) const;

public:

    static cFileSystem *Open ( const char *, const char * );

    static bool IsValidName ( const char *name );
    static bool IsValidFDR ( const sFileDescriptorRecord *fdr );

    virtual bool CheckDisk () const;
    virtual void ShowDirectory ( FILE *, bool ) const;
    virtual int  GetFilenames ( char *names[], int = -1 ) const;

    // Functions used by cFile
    virtual sSector *GetFileSector ( sFileDescriptorRecord *FDR, int index ) = 0;
    virtual int ExtendFile ( sFileDescriptorRecord *FDR, int count ) = 0;
    virtual void TruncateFile ( sFileDescriptorRecord *FDR, int limit ) = 0;
    virtual void DiskModified () = 0;

    // Generic file system functions
    virtual bool GetPath ( char *, size_t ) const = 0;
    virtual bool GetName ( char *, size_t ) const = 0;
    virtual bool IsValid () const = 0;
    virtual bool IsCollection () const = 0;
    virtual cFile *OpenFile ( const char *, int = -1 ) = 0;
    virtual cFile *CreateFile ( const char *, UINT8, int, int = -1 ) = 0;
    virtual bool AddFile ( cFile *, int = -1 ) = 0;
    virtual bool DeleteFile ( const char *, int = -1 ) = 0;

};

#endif
