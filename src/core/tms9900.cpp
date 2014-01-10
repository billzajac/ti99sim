//----------------------------------------------------------------------------
//
// File:        tms9900.cpp
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "opcodes.hpp"

DBG_REGISTER ( __FILE__ );

extern "C" {

    void Run ();
    void Stop ();
    bool Step ();
    bool IsRunning ();
    void ContextSwitch ( UINT16 address );

    extern UINT16  parity [ 256 ];
    extern sLookUp LookUp [ 16 ];
    extern UINT8   MemFlags  [ 0x10000 ];
    extern UINT16  InterruptFlag;
    extern UINT16  WorkspacePtr;
    extern UINT16  ProgramCounter;
    extern UINT16  Status;
    extern UINT32   InstructionCounter;
    extern UINT32   ClockCycleCounter;

    int (*TimerHook) ();

}

UINT16 parity [ 256 ];

BREAKPOINT_FUNCTION DebugHandler;
void               *DebugToken;

UINT8 MemTrapIndex [ 0x10000 ];

sTrapInfo TrapList [ 16 ];

sLookUp LookUp [ 16 ];

sOpCode OpCodes [ 69 ] = {
  { "A   ", 0xA000, 0xF000, 1, ( UINT16 ) -1, opcode_A    , 14,   4 },	// 14
  { "AB  ", 0xB000, 0xF000, 1, ( UINT16 ) -1, opcode_AB   , 14,   2 },	// 14
  { "ABS ", 0x0740, 0xFFC0, 6, ( UINT16 ) -1, opcode_ABS  , 12,   0 },	// 12/14
  { "AI  ", 0x0220, 0xFFE0, 8, ( UINT16 ) -1, opcode_AI   , 14,  15 },	// 14
  { "ANDI", 0x0240, 0xFFE0, 8, ( UINT16 ) -1, opcode_ANDI , 14,  31 },	// 14
  { "B   ", 0x0440, 0xFFC0, 6, ( UINT16 ) -1, opcode_B    ,  8, 126 },	// 8
  { "BL  ", 0x0680, 0xFFC0, 6, ( UINT16 ) -1, opcode_BL   , 12,  36 },	// 12
  { "BLWP", 0x0400, 0xFFC0, 6, ( UINT16 ) -1, opcode_BLWP , 26,   0 },	// 26
  { "C   ", 0x8000, 0xF000, 1, ( UINT16 ) -1, opcode_C    , 14,  14 },	// 14
  { "CB  ", 0x9000, 0xF000, 1, ( UINT16 ) -1, opcode_CB   , 14,   1 },	// 14
  { "CI  ", 0x0280, 0xFFE0, 8, ( UINT16 ) -1, opcode_CI   , 14,  47 },	// 14
  { "CKOF", 0x03C0, 0xFFFF, 7, ( UINT16 ) -1, opcode_CKOF , 12,   0 },	// 12
  { "CKON", 0x03A0, 0xFFFF, 7, ( UINT16 ) -1, opcode_CKON , 12,   0 },	// 12
  { "CLR ", 0x04C0, 0xFFC0, 6, ( UINT16 ) -1, opcode_CLR  , 10,  21 },	// 10
  { "COC ", 0x2000, 0xFC00, 3, ( UINT16 ) -1, opcode_COC  , 14,   0 },	// 14
  { "CZC ", 0x2400, 0xFC00, 3, ( UINT16 ) -1, opcode_CZC  , 14,   0 },	// 14
  { "DEC ", 0x0600, 0xFFC0, 6, ( UINT16 ) -1, opcode_DEC  , 10,  60 },	// 10
  { "DECT", 0x0640, 0xFFC0, 6, ( UINT16 ) -1, opcode_DECT , 10,   1 },	// 10
  { "DIV ", 0x3C00, 0xFC00, 9, ( UINT16 ) -1, opcode_DIV  , 16,   0 },	// 16/92-124
  { "IDLE", 0x0340, 0xFFFF, 7, ( UINT16 ) -1, opcode_IDLE , 12,   0 },	// 12
  { "INC ", 0x0580, 0xFFC0, 6, ( UINT16 ) -1, opcode_INC  , 10,  53 },	// 10
  { "INCT", 0x05C0, 0xFFC0, 6, ( UINT16 ) -1, opcode_INCT , 10,   9 },	// 10
  { "INV ", 0x0540, 0xFFC0, 6, ( UINT16 ) -1, opcode_INV  , 10,   2 },	// 10
  { "JEQ ", 0x1300, 0xFF00, 2, ( UINT16 ) -1, opcode_JEQ  ,  8,  39 },	// 8/10
  { "JGT ", 0x1500, 0xFF00, 2, ( UINT16 ) -1, opcode_JGT  ,  8,  19 },	// 10
  { "JH  ", 0x1B00, 0xFF00, 2, ( UINT16 ) -1, opcode_JH   ,  8,   0 },	// 10
  { "JHE ", 0x1400, 0xFF00, 2, ( UINT16 ) -1, opcode_JHE  ,  8,   4 },	// 10
  { "JL  ", 0x1A00, 0xFF00, 2, ( UINT16 ) -1, opcode_JL   ,  8,  18 },	// 10
  { "JLE ", 0x1200, 0xFF00, 2, ( UINT16 ) -1, opcode_JLE  ,  8,   0 },	// 10
  { "JLT ", 0x1100, 0xFF00, 2, ( UINT16 ) -1, opcode_JLT  ,  8,  70 },	// 10
  { "JMP ", 0x1000, 0xFF00, 2, ( UINT16 ) -1, opcode_JMP  ,  8, 200 },	// 10
  { "JNC ", 0x1700, 0xFF00, 2, ( UINT16 ) -1, opcode_JNC  ,  8,  27 },	// 10
  { "JNE ", 0x1600, 0xFF00, 2, ( UINT16 ) -1, opcode_JNE  ,  8,  93 },	// 10
  { "JNO ", 0x1900, 0xFF00, 2, ( UINT16 ) -1, opcode_JNO  ,  8,   0 },	// 10
  { "JOC ", 0x1800, 0xFF00, 2, ( UINT16 ) -1, opcode_JOC  ,  8,   3 },	// 10
  { "JOP ", 0x1C00, 0xFF00, 2, ( UINT16 ) -1, opcode_JOP  ,  8,   0 },	// 10
  { "LDCR", 0x3000, 0xFC00, 4, ( UINT16 ) -1, opcode_LDCR , 20,   1 },	// 20+2*bits
  { "LI  ", 0x0200, 0xFFE0, 8, ( UINT16 ) -1, opcode_LI   , 12,  21 },	// 12
  { "LIMI", 0x0300, 0xFFE0, 8, ( UINT16 ) -1, opcode_LIMI , 16,  60 },	// 16
  { "LREX", 0x03E0, 0xFFFF, 7, ( UINT16 ) -1, opcode_LREX , 12,   0 },	// 12
  { "LWPI", 0x02E0, 0xFFE0, 8, ( UINT16 ) -1, opcode_LWPI , 10,   1 },	// 10
  { "MOV ", 0xC000, 0xF000, 1, ( UINT16 ) -1, opcode_MOV  , 14, 144 },	// 14
  { "MOVB", 0xD000, 0xF000, 1, ( UINT16 ) -1, opcode_MOVB , 14, 420 },	// 14
  { "MPY ", 0x3800, 0xFC00, 9, ( UINT16 ) -1, opcode_MPY  , 52,   0 },	// 52
  { "NEG ", 0x0500, 0xFFC0, 6, ( UINT16 ) -1, opcode_NEG  , 12,   0 },	// 12
  { "ORI ", 0x0260, 0xFFE0, 8, ( UINT16 ) -1, opcode_ORI  , 14,  25 },	// 14
  { "RSET", 0x0360, 0xFFFF, 7, ( UINT16 ) -1, opcode_RSET , 12,   0 },	// 12
  { "RTWP", 0x0380, 0xFFFF, 7, ( UINT16 ) -1, opcode_RTWP , 14,   0 },	// 14
  { "S   ", 0x6000, 0xF000, 1, ( UINT16 ) -1, opcode_S    , 14,   2 },	// 14
  { "SB  ", 0x7000, 0xF000, 1, ( UINT16 ) -1, opcode_SB   , 14,   0 },	// 14
  { "SBO ", 0x1D00, 0xFF00, 2, ( UINT16 ) -1, opcode_SBO  , 12,   0 },	// 12
  { "SBZ ", 0x1E00, 0xFF00, 2, ( UINT16 ) -1, opcode_SBZ  , 12,   0 },	// 12
  { "SETO", 0x0700, 0xFFC0, 6, ( UINT16 ) -1, opcode_SETO , 10,   5 },	// 10
  { "SLA ", 0x0A00, 0xFF00, 5, ( UINT16 ) -1, opcode_SLA  , 12,  55 },	// 12+2*disp/20+2*disp
  { "SOC ", 0xE000, 0xF000, 1, ( UINT16 ) -1, opcode_SOC  , 14,   2 },	// 14
  { "SOCB", 0xF000, 0xF000, 1, ( UINT16 ) -1, opcode_SOCB , 14,   0 },	// 14
  { "SRA ", 0x0800, 0xFF00, 5, ( UINT16 ) -1, opcode_SRA  , 12,  16 },	// 12+2*disp/20+2*disp
  { "SRC ", 0x0B00, 0xFF00, 5, ( UINT16 ) -1, opcode_SRC  , 12,   0 },	// 12+2*disp/20+2*disp
  { "SRL ", 0x0900, 0xFF00, 5, ( UINT16 ) -1, opcode_SRL  , 12,  60 },	// 12+2*disp/20+2*disp
  { "STCR", 0x3400, 0xFC00, 4, ( UINT16 ) -1, opcode_STCR , 42,   1 },	// 42/44/58/60
  { "STST", 0x02C0, 0xFFE0, 8, ( UINT16 ) -1, opcode_STST ,  8,   3 },	// 8
  { "STWP", 0x02A0, 0xFFE0, 8, ( UINT16 ) -1, opcode_STWP ,  8,   0 },	// 8
  { "SWPB", 0x06C0, 0xFFC0, 6, ( UINT16 ) -1, opcode_SWPB , 10,  24 },	// 10
  { "SZC ", 0x4000, 0xF000, 1, ( UINT16 ) -1, opcode_SZC  , 14,   1 },	// 14
  { "SZCB", 0x5000, 0xF000, 1, ( UINT16 ) -1, opcode_SZCB , 14,  16 },	// 14
  { "TB  ", 0x1F00, 0xFF00, 2, ( UINT16 ) -1, opcode_TB   , 12,   0 },	// 12
  { "X   ", 0x0480, 0xFFC0, 6, ( UINT16 ) -1, opcode_X    ,  8,   0 },	// 8
  { "XOP ", 0x2C00, 0xFC00, 9, ( UINT16 ) -1, opcode_XOP  , 36,   0 },	// 36
  { "XOR ", 0x2800, 0xFC00, 3, ( UINT16 ) -1, opcode_XOR  , 14,   0 } 	// 14
};

int cTMS9900::sortFunction ( const sOpCode *p1, const sOpCode *p2 )
{
    int op1 = p1->opCode & 0xF000;
    int op2 = p2->opCode & 0xF000;

    if ( op1 < op2 ) return -1;
    if ( op1 > op2 ) return 1;

    if ( p1->count > p2->count ) return -1;
    if ( p1->count < p2->count ) return 1;

    if ( p1->opCode < p2->opCode ) return -1;
    if ( p1->opCode > p2->opCode ) return 1;

    return 0;
}

extern "C" UINT8 CallTrapB ( bool read, ADDRESS address, UINT8 value )
{
    FUNCTION_ENTRY ( NULL, "CallTrapB", false );

    int index = MemTrapIndex [ address ];

    sTrapInfo *pInfo = &TrapList [ index ];

    if ( MemFlags [address] & MEMFLG_DEBUG ) {
        DBG_ASSERT ( DebugHandler != NULL );
        if (( MemFlags [address] & MEMFLG_TRAP_ACCESS ) == 0 ) pInfo = NULL;
        return DebugHandler ( DebugToken, address, false, value, read, false, pInfo );
    }

    return pInfo->function ( pInfo->ptr, pInfo->data, read, address, value );
}

extern "C" UINT16 CallTrapW ( bool read, bool isFetch, ADDRESS address, UINT16 value )
{
    FUNCTION_ENTRY ( NULL, "CallTrapW", false );

    int index = MemTrapIndex [ address ];

    sTrapInfo *pInfo = &TrapList [ index ];

    if (( MemFlags [address] | MemFlags [address+1] ) & MEMFLG_DEBUG ) {
        DBG_ASSERT ( DebugHandler != NULL );
        if (( MemFlags [address] & MEMFLG_TRAP_ACCESS ) == 0 ) pInfo = NULL;
        return DebugHandler ( DebugToken, address, true, value, read, isFetch, pInfo );
    }

    return ( UINT16 ) (( pInfo->function ( pInfo->ptr, pInfo->data, read, address, ( UINT8 ) ( value >> 8 )) << 8 ) | ( UINT8 ) ( value ));
}

extern "C" UINT8  CpuMemory [ 0x10000 ];

extern "C" void InvalidOpcode ()
{
    FUNCTION_ENTRY ( NULL, "InvalidOpcode", true );

    DBG_ERROR ( "PC = " << hex << ( UINT16 ) ( ProgramCounter - 2 ) << " OpCode: " << CpuMemory [ProgramCounter-2] << CpuMemory [ProgramCounter-1] );
}

cTMS9900::cTMS9900 ()
{
    FUNCTION_ENTRY ( this, "cTMS9900 ctor", true );

    // Fill in the parity table
    for ( unsigned i = 0; i < SIZE ( parity ); i++ ) {
        int value = 0, bits = i;
        for ( int x = 0; x < 8; x++ ) {
            value ^= bits;
            bits >>= 1;
        }
        parity [ i ] = ( value & 1 ) ? TMS_PARITY : 0;
    }

    // Sort the OpCode table by OpCode
    qsort ( OpCodes, SIZE ( OpCodes ), sizeof ( OpCodes[0] ), ( QSORT_FUNC ) sortFunction );

    // Adjust clock count information to account for minimum values (so that Illegal op-codes will count correctly)
    for ( unsigned i = 0; i < SIZE ( OpCodes ); i++ ) {
        OpCodes [i].count = 0;
    }

    // Create the LookUp table using the high 4 bits of each OpCode
    int last = 0;
    unsigned x;
    for ( x = 0; x < SIZE ( LookUp ) - 1; x++ ) {
        int index = last + 1;
        while ( x + 1 > ( unsigned ) ( OpCodes[index].opCode >> 12 )) index++;
        LookUp [x].opCode = &OpCodes [last];
        LookUp [x].size = index - last - 1;
        last = index;
    }
    LookUp [x].opCode = &OpCodes [last];
    LookUp [x].size = SIZE ( OpCodes ) - last - 1;

    memset ( MemFlags, MEMFLG_8BIT, sizeof ( MemFlags ));

    // Mark off the memory regions that are 16-bit (for access cycle counting)
    for ( unsigned i = 0x0000; i < 0x2000; i++ ) MemFlags [i] &= ~MEMFLG_8BIT;
    for ( unsigned i = 0x8000; i < 0x8400; i++ ) MemFlags [i] &= ~MEMFLG_8BIT;

    Reset ();
}

cTMS9900::~cTMS9900 ()
{
    FUNCTION_ENTRY ( this, "cTMS9900 dtor", true );
}

void cTMS9900::Reset ()
{
    SetPC ( 0x0000 );
    SetWP ( 0x0000 );
    SetST ( 0x0000 );

    // Simulate a hardware powerup
    ContextSwitch ( 0 );
}

void cTMS9900::SignalInterrupt ( UINT8 level )  { InterruptFlag |= 1 << level; }
void cTMS9900::ClearInterrupt ( UINT8 level )   { InterruptFlag &= ~ ( 1 << level ); }
void cTMS9900::SetPC ( ADDRESS address )        { ProgramCounter = address; }
void cTMS9900::SetWP ( ADDRESS address )        { WorkspacePtr = address; }
void cTMS9900::SetST ( UINT16  value )          { Status = value; }

ADDRESS cTMS9900::GetPC ()                      { return ProgramCounter; }
ADDRESS cTMS9900::GetWP ()                      { return WorkspacePtr; }
UINT16  cTMS9900::GetST ()                      { return Status; }

void cTMS9900::Run ()                           { ::Run (); }
void cTMS9900::Stop ()                          { ::Stop (); }
bool cTMS9900::Step ()                          { return ::Step (); }
bool cTMS9900::IsRunning ()                     { return ::IsRunning (); }

UINT32 cTMS9900::GetClocks ()                   { return ClockCycleCounter; }
void  cTMS9900::AddClocks ( int clocks )        { ClockCycleCounter += clocks; }
void  cTMS9900::ResetClocks ()                  { ClockCycleCounter = 0; }

UINT32 cTMS9900::GetCounter ()                  { return InstructionCounter; }
void  cTMS9900::ResetCounter ()                 { InstructionCounter = 0; }

bool cTMS9900::SaveImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cTMS9900::SaveImage", true );

    if (( fwrite ( &WorkspacePtr, sizeof ( ADDRESS ), 1, file ) != 1 )        ||
        ( fwrite ( &ProgramCounter, sizeof ( ADDRESS ), 1, file ) != 1 )      ||
        ( fwrite ( &Status, sizeof ( UINT16 ), 1, file ) != 1 )               ||
        ( fwrite ( &InterruptFlag, sizeof ( InterruptFlag ), 1, file ) != 1 ) ||
        ( fwrite ( &InstructionCounter, sizeof ( InstructionCounter ), 1, file ) != 1 ) ||
        ( fwrite ( &ClockCycleCounter, sizeof ( ClockCycleCounter ), 1, file ) != 1 )) {
        DBG_ERROR ( "Unable to save image to file" );
        return false;
    }
    for ( unsigned i = 0; i < SIZE ( OpCodes ); i++ ) {
        if ( fwrite ( &OpCodes[i].count, sizeof ( OpCodes[i].count ), 1, file ) != 1 ) {
            DBG_ERROR ( "Unable to save image to file" );
            return false;
        }
    }

    return true;
}

bool cTMS9900::LoadImage ( FILE *file )
{
    FUNCTION_ENTRY ( this, "cTMS9900::LoadImage", true );

    if (( fread ( &WorkspacePtr, sizeof ( ADDRESS ), 1, file ) != 1 )        ||
        ( fread ( &ProgramCounter, sizeof ( ADDRESS ), 1, file ) != 1 )      ||
        ( fread ( &Status, sizeof ( UINT16 ), 1, file ) != 1 )               ||
        ( fread ( &InterruptFlag, sizeof ( InterruptFlag ), 1, file ) != 1 ) ||
        ( fread ( &InstructionCounter, sizeof ( InstructionCounter ), 1, file ) != 1 ) ||
        ( fread ( &ClockCycleCounter, sizeof ( ClockCycleCounter ), 1, file ) != 1 )) {
        DBG_ERROR ( "Unable to load image from file" );
        return false;
    }

    DBG_STATUS ( "WP: " << hex << WorkspacePtr );
    DBG_STATUS ( "PC: " << hex << ProgramCounter );
    DBG_STATUS ( "ST: " << hex << Status );
    DBG_STATUS ( "Int Flag: " << InterruptFlag );
    DBG_STATUS ( "Inst Cnt: " << InstructionCounter );
    DBG_STATUS ( "Cycle Cnt: " << ClockCycleCounter );

    for ( unsigned i = 0; i < SIZE ( OpCodes ); i++ ) {
        if ( fread ( &OpCodes[i].count, sizeof ( OpCodes[i].count ), 1, file ) != 1 ) {
            DBG_ERROR ( "Unable to load image from file" );
            return false;
        }
    }

    return true;
}

UINT8 cTMS9900::RegisterTrapHandler ( TRAP_FUNCTION function, void *ptr, int data )
{
    FUNCTION_ENTRY ( this, "cTMS9900::RegisterTrapHandler", true );

    for ( UINT8 i = 1; i < ( int ) SIZE ( TrapList ); i++ ) {
        if ( TrapList [i].ptr == NULL ) {
            TrapList [i].ptr      = ptr;
            TrapList [i].data     = data;
            TrapList [i].function = function;
            return i;
        }
    }
    return ( UINT8 ) -1;
}

void cTMS9900::DeRegisterTrapHandler ( UINT8 index )
{
    FUNCTION_ENTRY ( this, "cTMS9900::DeRegisterTrapHandler", true );

    if (( index == 0 ) || ( index >= SIZE ( TrapList ))) return;

    ClearTrap ( index );

    TrapList [index].ptr      = NULL;
    TrapList [index].data     = 0;
    TrapList [index].function = NULL;
}

UINT8 cTMS9900::GetTrapIndex ( TRAP_FUNCTION function, int data )
{
    FUNCTION_ENTRY ( this, "cTMS9900::GetTrapIndex", true );

    for ( UINT8 i = 1; i < SIZE ( TrapList ); i++ ) {
        if (( TrapList [i].function == function ) && ( TrapList [i].data == data )) return i;
    }

    return ( UINT8 ) -1;
}

bool cTMS9900::SetTrap ( ADDRESS address, UINT8 type, UINT8 index )
{
    FUNCTION_ENTRY ( this, "cTMS9900::SetTrap", false );

    if (( index == 0 ) || ( index >= SIZE ( TrapList ))) return false;
    if (( type == 0 ) || ( MemFlags [ address ] & MEMFLG_TRAP_ACCESS )) return false;

    MemFlags [ address ] |= type;
    MemTrapIndex [ address ] = index;

    return true;
}

void cTMS9900::ClearTrap ( UINT8 index )
{
    FUNCTION_ENTRY ( this, "cTMS9900::ClearTrap", true );

    if (( index == 0 ) || ( index >= SIZE ( TrapList ))) return;

    for ( long offset = 0; offset < 0x10000; offset++ ) {
        if ( MemTrapIndex [ offset ] == index ) {
            MemFlags [ offset ] &= ( UINT8 ) ~ MEMFLG_TRAP_ACCESS;
        }
    }
}

void cTMS9900::SetMemory ( MEMORY_TYPE_E type, ADDRESS offset, int length )
{
    FUNCTION_ENTRY ( this, "cTMS9900::SetMemory", true );

    for ( int i = 0; i < length; i++ ) {
        if ( type == MEM_ROM ) {
            MemFlags [ offset++ ] |= MEMFLG_ROM;
        } else if ( type == MEM_RAM ) {
            MemFlags [ offset++ ] &= ~MEMFLG_ROM;
        } else {
            MemFlags [ offset++ ] |= MEMFLG_SCRATCHPAD;
        }
    }
}

void cTMS9900::RegisterDebugHandler ( BREAKPOINT_FUNCTION handler, void *token )
{
    FUNCTION_ENTRY ( this, "cTMS9900::RegisterDebugHandler", true );

    DebugHandler = handler;
    DebugToken   = token;
}

void cTMS9900::DeRegisterDebugHandler ()
{
    FUNCTION_ENTRY ( this, "cTMS9900::DeRegisterDebugHandler", true );

    DebugHandler = NULL;
    DebugToken   = NULL;

    for ( int i = 0; i < 0x10000; i++ ) {
        MemFlags [i] &= ( UINT8 ) ~MEMFLG_DEBUG;
    }
}

bool cTMS9900::SetBreakpoint ( ADDRESS address, UINT8 flags )
{
    FUNCTION_ENTRY ( this, "cTMS9900::SetBreakpoint", true );

    if (( flags & MEMFLG_DEBUG ) != flags ) return false;

    MemFlags [ address ] |= flags;

    return true;
}

bool cTMS9900::ClearBreakpoint ( ADDRESS address, UINT8 flags )
{
    FUNCTION_ENTRY ( this, "cTMS9900::ClearBreakpoint", true );

    if (( flags & MEMFLG_DEBUG ) != flags ) return false;

    if (( MemFlags [ address ] & flags ) != flags ) return false;

    MemFlags [ address ] &= ( UINT8 ) ~flags;

    return true;
}
