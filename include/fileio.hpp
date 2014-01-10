//----------------------------------------------------------------------------
//
// File:        fileio.hpp
// Date:        19-Sep-2000
// Programmer:  Marc Rousseau
//
// Description: This file contains startup code for Linux/SDL
//
// Copyright (c) 2000-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef FILEIO_HPP_
#define FILEIO_HPP_

#include "diskfs.hpp"
#include "cBaseObject.hpp"

class cFile : public cBaseObject {

    friend class cFileSystem;

    cFileSystem             *m_FileSystem;
    sFileDescriptorRecord   *m_FDR;
    int                      m_TotalRecordsLeft;
    int                      m_RecordsLeft;
    int                      m_SectorIndex;
    UINT8                    m_SectorBuffer [256];
    UINT8                   *m_RecordPtr;

private:

    // Disable the copy constructor and assignment operator defaults
    cFile ( const cFile & );			// no implementation
    void operator = ( const cFile & );		// no implementation

protected:

    cFile ( cFileSystem *, sFileDescriptorRecord * );
    ~cFile ();

    bool ReadNextSector ();

public:

    static cFile *Open ( const char *filename, const char *path = NULL );

    int FileSize ();

    bool GetPath ( char *, size_t ) const;
    bool GetName ( char *, size_t ) const;

    sFileDescriptorRecord *GetFDR ();

    bool SeekRecord ( int );
    int ReadRecord ( void *, int );
    int WriteRecord ( const void *, int );

    int ReadSector ( int, void * );
    int WriteSector ( int, const void * );

};

inline sFileDescriptorRecord *cFile::GetFDR ()          { return m_FDR; }

#endif
