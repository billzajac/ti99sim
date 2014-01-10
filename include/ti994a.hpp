//----------------------------------------------------------------------------
//
// File:        ti994a.hpp
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

#ifndef TI994A_HPP_
#define TI994A_HPP_

#include "tms9900.hpp"

class  cTMS5220;
class  cTMS9900;
class  cTMS9901;
class  cTMS9918A;
class  cTMS9919;
class  cCartridge;
class  cDevice;

const int CPU_SPEED_HZ = 3000000;

struct sMemoryRegion;

enum HEADER_SECTION_E {
    SECTION_BASE,
    SECTION_CPU,
    SECTION_VDP,
    SECTION_GROM,
    SECTION_ROM,
    SECTION_SOUND,
    SECTION_SPEECH,
    SECTION_CRU,
    SECTION_DSR
};

struct sStateHeader {
    UINT16          id;
    UINT16          length;
};

struct sStateHeaderInfo {
    sStateHeader    header;
    UINT32          offset;
};

struct sImageFileState {
    FILE           *file;
    long            start;
    long            next;
};

class cTI994A {

protected:

    enum TRAP_TYPE_E {
        TRAP_BANK_SWITCH,
        TRAP_SCRATCH_PAD,
        TRAP_SOUND,
        TRAP_SPEECH,
        TRAP_VIDEO,
        TRAP_GROM
    };

    cTMS9900           *m_CPU;
    cTMS9901           *m_PIC;
    cTMS9918A          *m_VDP;
    cTMS9919           *m_SoundGenerator;
    cTMS5220           *m_SpeechSynthesizer;

    UINT32              m_RetraceInterval;
    UINT32              m_LastRetrace;

    cCartridge         *m_Console;
    cCartridge         *m_Cartridge;

    UINT16              m_ActiveCRU;
    cDevice            *m_Device [32];

    UINT8              *m_GromPtr;
    ADDRESS             m_GromAddress;
    ADDRESS             m_GromLastInstruction;
    int                 m_GromReadShift;
    int                 m_GromWriteShift;
    int                 m_GromCounter;

    sMemoryRegion      *m_CpuMemoryInfo [16];	// Pointers 4K banks of CPU RAM
    sMemoryRegion      *m_GromMemoryInfo [8];	// Pointers 8K banks of Graphics RAM

    UINT8              *m_CpuMemory;		// Pointer to 64K of System ROM/RAM
    UINT8              *m_GromMemory;		// Pointer to 64K of Graphics ROM/RAM
    UINT8              *m_VideoMemory;		// Pointer to 16K of Video RAM

public:

    cTI994A ( cCartridge *, cTMS9918A * = NULL, cTMS9919 * = NULL, cTMS5220 * = NULL );
    virtual ~cTI994A ();

    cTMS9900  *GetCPU ()			{ return m_CPU; }
    cTMS9918A *GetVDP ()			{ return m_VDP; }
    cTMS9919  *GetSoundGenerator ()		{ return m_SoundGenerator; }

    UINT8   *GetCpuMemory () const		{ return m_CpuMemory; }
    UINT8   *GetGromMemory () const		{ return m_GromMemory; }
    UINT8   *GetVideoMemory () const		{ return m_VideoMemory; }

    ADDRESS  GetGromAddress () const		{ return m_GromAddress; }
    void     SetGromAddress ( ADDRESS addr )	{ m_GromAddress = addr; m_GromPtr = m_GromMemory + addr; }

    virtual void Sleep ( int, UINT32 )		{}
    virtual void WakeCPU ( UINT32 )		{}

    virtual int  ReadCRU ( ADDRESS );
    virtual void WriteCRU ( ADDRESS, UINT16 );

    virtual void AddDevice ( cDevice * );

    virtual void InsertCartridge ( cCartridge *, bool = true );
    virtual void RemoveCartridge ( cCartridge *, bool = true );

    virtual void Reset ();

    virtual void Run ();
    virtual void Stop ();
    virtual bool Step ();
    virtual bool IsRunning ();

    virtual void Refresh ( bool )		{}

    static bool OpenImageFile ( const char *, sImageFileState * );
    static bool FindHeader ( sImageFileState *, HEADER_SECTION_E );
    static void MarkHeader ( FILE *, HEADER_SECTION_E, sStateHeaderInfo * );
    static bool SaveHeader ( FILE *, sStateHeaderInfo * );

    virtual bool SaveImage ( const char * );
    virtual bool LoadImage ( const char * );

protected:

    cDevice *GetDevice ( ADDRESS ) const;

    static int _TimerHookProc ();
    virtual int TimerHookProc ();

    static UINT8 TrapFunction ( void *, int, bool, ADDRESS, UINT8 );

    virtual UINT8 BankSwitch            ( ADDRESS, UINT8 );
    virtual UINT8 SoundBreakPoint       ( ADDRESS, UINT8 );
    virtual UINT8 SpeechWriteBreakPoint ( ADDRESS, UINT8 );
    virtual UINT8 SpeechReadBreakPoint  ( ADDRESS, UINT8 );
    virtual UINT8 VideoWriteBreakPoint  ( ADDRESS, UINT8 );
    virtual UINT8 VideoReadBreakPoint   ( ADDRESS, UINT8 );
    virtual UINT8 GromWriteBreakPoint   ( ADDRESS, UINT8 );
    virtual UINT8 GromReadBreakPoint    ( ADDRESS, UINT8 );

private:

    cTI994A ( const cTI994A & );          // no implementation
    void operator = ( const cTI994A & );  // no implementation

};

#endif
