//----------------------------------------------------------------------------
//
// File:        pseudofs.hpp
// Date:        05-Sep-2003
// Programmer:  Marc Rousseau
//
// Description: A class to simulate a TI disk filesystem for TIFILES & v9t9 FIAD files
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

#ifndef PSEUDOFS_HPP_
#define PSEUDOFS_HPP_

#include "fs.hpp"

/*
> - 1) The first, the most common one, uses just the first 16 bytes:
>
> 00000000h: 07 54 49 46 49 4C 45 53 01 55 08 02 00 80 AA 02 ;
> .TIFILES.U..._a.
> 00000010h: CA 53 CA 53 CA 53 CA 53 CA 53 CA 53 CA 53 CA 53 ;
> ESESESESESESESES
>
> - 2) The second adds the original TI filename, as an ASCIIZ string,
> starting from the 16th byte, and pads with BLANKS up to the 79th byte :
>
> File: E:\ARKs\Odd-Arks\ARTICLS1.ARK 14/01/1994, 23.11.08
>
> 00000000h: 07 54 49 46 49 4C 45 53 00 8D 02 02 00 80 1A 01
> ;.TIFILES._..._..
> 00000010h: 24 41 52 54 49 43 4C 45 53 31 00 20 20 20 20 20 ;$ARTICLES1.
> 00000020h: 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 ;
>
> - 3) The third adds the original TI filename starting from the 26th byte
> There are 10 extra bytes in between (create and update time?). The
> remaining part is padded with zeroes up to the 79th byte.
>
> File: E:\ARKs\Odd-Arks\ACEASM.ARK 14/01/1994, 20.54.28
>
> 00000000h: 07 54 49 46 49 4C 45 53 00 1C 01 00 4E 00 00 00
> ;.TIFILES....N...
> 00000010h: 00 14 12 00 50 00 00 00 01 14 41 43 45 2F 41 53
> ;....P.....ACE/AS
> 00000020h: 4D 20 20 20 20 20 00 00 00 00 00 00 00 00 00 00 ;M
> ..........
> 00000030h: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
> ;................
>
> I've come across yet another variation of type 2. The original TI filename
> is terminated with 0D, 0A, as if it were a DOS TXT record, and padded
> with BLANKS up to the 79th byte.
>
*/

/*
>The format is:
>
>  Byte                   Meaning
>-------  --------------------------------------------
> 0       string length 07
> 1-7     "TIFILES" string
> 8-9     # of sectors allocated to file
> 10      status flags
 > 11      # of records per sector
> 12      EOF offset (# of bytes used in last sector)
> 13      logical record size
> 14-15   # of level 3 records allocated
> 16-17   MS bytes of total # of sector and # of level 3 records
*/

struct sTIFILES_Header {
    UINT8       length;              // String length (7)
    char        Name [7];            // "TIFILES" string
    UINT16      SectorCount;
    UINT8       Status;
    UINT8       RecordsPerSector;
    UINT8       EOF_Offset;
    UINT8       RecordSize;
    UINT16      RecordCount;
    UINT16      unknown;
};

class cPseudoFileSystem : public cFileSystem {

    char                    *m_PathName;
    char                    *m_FileName;
    UINT8                   *m_FileBuffer;
    FILE                    *m_File;
    sFileDescriptorRecord    m_FDR;
    sSector                 *m_CurrentSector;

protected:

    cPseudoFileSystem ( const char * );
    ~cPseudoFileSystem ();

    bool FindHeader ();
    bool ConstructFDR_TIFILES ( char * );
    bool ConstructFDR_FIAD ( char * );

    void LoadFileBuffer ();

    // cFileSystem methods
    virtual int FileCount ( int ) const;
    virtual const sFileDescriptorRecord * GetFileDescriptor ( int, int ) const;
    virtual int FreeSectors () const;
    virtual int TotalSectors () const;
    virtual sSector *GetFileSector ( sFileDescriptorRecord *FDR, int index );
    virtual int ExtendFile ( sFileDescriptorRecord *FDR, int count );
    virtual void TruncateFile ( sFileDescriptorRecord *FDR, int limit );
    virtual void DiskModified ();

public:

    static cPseudoFileSystem *Open ( const char *, const char * );

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

    cPseudoFileSystem ( const cPseudoFileSystem & );  // no implementation
    void operator = ( const cPseudoFileSystem & );    // no implementation

};

#endif
