//----------------------------------------------------------------------------
//
// File:        arcfs.hpp
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

#ifndef ARCFS_HPP_
#define ARCFS_HPP_

#include "fs.hpp"

class cPseudoFileSystem;
class cDecodeLZW;

class cArchiveFileSystem : public cFileSystem {

    struct sFileInfo {
        sFileDescriptorRecord  fdr;
        sSector               *sector;
        UINT8                 *fileData;
        UINT32                 bytesUsed;
    };

    cPseudoFileSystem         *m_Container;
    sFileInfo                  m_Directory [128];
    int                        m_FileCount;
    int                        m_TotalSectors;

protected:

    cArchiveFileSystem ( cPseudoFileSystem * );
    ~cArchiveFileSystem ();

    static bool DirectoryCallback ( void *, size_t, void * );
    static bool DataCallback ( void *, size_t, void * );

    void LoadFile ();

    const sFileInfo *GetFileInfo ( const sFileDescriptorRecord * ) const;

    // cFileSystem non-public methods
    virtual int FileCount ( int ) const;
    virtual const sFileDescriptorRecord * GetFileDescriptor ( int, int ) const;
    virtual int FreeSectors () const;
    virtual int TotalSectors () const;
    virtual const char *VerboseHeader ( int ) const;
    virtual void PrintVerboseInformation ( FILE *, const sFileDescriptorRecord *, int ) const;
    virtual sSector *GetFileSector ( sFileDescriptorRecord *FDR, int index );
    virtual int ExtendFile ( sFileDescriptorRecord *FDR, int count );
    virtual void TruncateFile ( sFileDescriptorRecord *FDR, int limit );
    virtual void DiskModified ();

public:

    static cArchiveFileSystem *Open ( const char *, const char * );

    // cFileSystem public methods
    virtual bool GetPath ( char *, size_t ) const;
    virtual bool GetName ( char *, size_t ) const;
    virtual bool IsValid () const;
    virtual bool IsCollection () const;
    virtual cFile *OpenFile ( const char *, int );
    virtual cFile *CreateFile ( const char *, UINT8, int, int );
    virtual bool AddFile ( cFile *, int );
    virtual bool DeleteFile ( const char *, int );

private:

    cArchiveFileSystem ( const cArchiveFileSystem & ); // no implementation
    void operator = ( const cArchiveFileSystem & );    // no implementation

};

#endif
