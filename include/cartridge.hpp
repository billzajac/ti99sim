//----------------------------------------------------------------------------
//
// File:        cartridge.hpp
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

#ifndef CARTRIDGE_HPP_
#define CARTRIDGE_HPP_

#define ROM_BANK_SIZE	0x1000
#define GROM_BANK_SIZE	0x2000

enum BANK_TYPE_E {
    BANK_UNKNOWN,
    BANK_RAM,
    BANK_ROM,
    BANK_BATTERY_BACKED,
    BANK_MAX
};

struct sMemoryBank {
    BANK_TYPE_E    Type;
    UINT8         *Data;
};

struct sMemoryRegion {
    int            NumBanks;
    sMemoryBank   *CurBank;
    sMemoryBank    Bank [4];
};


/// A container class for the TI-99/4A system ROM, device ROMs, ROM/GROM cartridges, GRAM Kracker, ...
/// The cCartridge class stores information about the various banks of memory in a particular device
/// that can be plugged into the TI-99/4A. Each bank can be either ROM, RAM, or battery-backed RAM.
///
class cCartridge {

    static const char  *sm_Banner;

    char          *m_FileName;
    char          *m_RamFileName;
    char          *m_Title;
    UINT16         m_BaseCRU;

// Manufacturer
// Copyright/date
// Catalog Number
// Icon/Image

    void SetFileName ( const char * );

    void DumpRegion ( FILE *, const char *, const sMemoryRegion *, unsigned, int, BANK_TYPE_E, bool ) const;

    bool LoadRAM () const;
    bool SaveRAM () const;

    static bool EncodeCallback ( void *, size_t, void * );
    static bool DecodeCallback ( void *, size_t, void * );

    static bool SaveBufferLZW ( void *, size_t, FILE * );

    static bool LoadBufferLZW ( void *, size_t, FILE * );
    static bool LoadBufferRLE ( void *, size_t, FILE * );

    bool LoadImageV0 ( FILE * );
    bool LoadImageV1 ( FILE * );
    bool LoadImageV2 ( FILE * );

public:

    sMemoryRegion CpuMemory [16];
    sMemoryRegion GromMemory [8];

    cCartridge ( const char * = NULL );
    ~cCartridge ();

    void SetTitle ( const char * );

    void SetCRU ( UINT16 cru )         { m_BaseCRU = cru; }
    UINT16 GetCRU () const             { return m_BaseCRU; }

    const char *Title () const         { return m_Title; }
    const char *FileName () const      { return m_FileName; }

    bool IsValid () const;

    bool LoadImage ( const char * );
    bool SaveImage ( const char * );

    void PrintInfo ( FILE * ) const;

private:

    cCartridge ( const cCartridge & );      // no implementation
    void operator = ( const cCartridge & ); // no implementation

};

#endif
