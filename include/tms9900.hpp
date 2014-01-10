//----------------------------------------------------------------------------
//
// File:        tms9900.hpp
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

#ifndef TMS9900_HPP_
#define TMS9900_HPP_

#if defined ( ADDRESS )
    #undef ADDRESS
#endif

struct sTrapInfo;

typedef unsigned short ADDRESS;

typedef UINT8 (*TRAP_FUNCTION) ( void *, int, bool, ADDRESS, UINT8 );
typedef UINT16 (*BREAKPOINT_FUNCTION) ( void *, ADDRESS, bool, UINT16, bool, bool, sTrapInfo * );

const int MEMFLG_ROM           = 0x01;
const int MEMFLG_SCRATCHPAD    = 0x02;
const int MEMFLG_8BIT          = 0x04;
const int MEMFLG_FETCH         = 0x08;
const int MEMFLG_READ          = 0x10;
const int MEMFLG_WRITE         = 0x20;
const int MEMFLG_DEBUG         = 0x38;
const int MEMFLG_TRAP_READ     = 0x40;
const int MEMFLG_TRAP_WRITE    = 0x80;
const int MEMFLG_TRAP_ACCESS   = 0xC0;

enum MEMORY_TYPE_E {
    MEM_ROM,
    MEM_RAM,
    MEM_PAD
};

struct sOpCode {
    char        mnemonic [8];
    UINT16      opCode;
    UINT16      mask;
    UINT16      format;
    UINT16      unused;
    void      (*function) ();
    UINT32      clocks;
    UINT32      count;
};

struct sLookUp {
    sOpCode    *opCode;
    int         size;
};

struct sTrapInfo {
    void          *ptr;
    int            data;
    TRAP_FUNCTION  function;
};

const int TMS_LOGICAL          = 0x8000;
const int TMS_ARITHMETIC       = 0x4000;
const int TMS_EQUAL            = 0x2000;
const int TMS_CARRY            = 0x1000;
const int TMS_OVERFLOW         = 0x0800;
const int TMS_PARITY           = 0x0400;
const int TMS_XOP              = 0x0200;

class cTMS9900 {

    static int sortFunction ( const sOpCode *p1, const sOpCode *p2 );

public:

    cTMS9900 ();
    ~cTMS9900 ();

    void SetPC ( ADDRESS address );
    void SetWP ( ADDRESS address );
    void SetST ( UINT16  address );

    ADDRESS GetPC ();
    ADDRESS GetWP ();
    UINT16  GetST ();

    void Run ();
    void Stop ();
    bool Step ();

    bool IsRunning ();

    void Reset ();
    void SignalInterrupt ( UINT8 );
    void ClearInterrupt ( UINT8 );

    UINT32 GetClocks ();
    void  AddClocks ( int );
    void  ResetClocks ();

    UINT32 GetCounter ();
    void  ResetCounter ();

    bool SaveImage ( FILE * );
    bool LoadImage ( FILE * );

    UINT8 RegisterTrapHandler ( TRAP_FUNCTION, void *, int );
    void DeRegisterTrapHandler ( UINT8 );

    UINT8 GetTrapIndex ( TRAP_FUNCTION, int );
    bool SetTrap ( ADDRESS, UINT8, UINT8 );
    void SetMemory ( MEMORY_TYPE_E, ADDRESS, int );

    void ClearTrap ( UINT8 );

    void RegisterDebugHandler ( BREAKPOINT_FUNCTION, void * );
    void DeRegisterDebugHandler ();
    bool SetBreakpoint ( ADDRESS, UINT8 );
    bool ClearBreakpoint ( ADDRESS, UINT8 );

};

#endif
