//----------------------------------------------------------------------------
//
// File:        opcodes.cpp
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
//   25-Jun-2000    MAR Added hack provided by Jeremy Stanley for MSVC bug
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "opcodes.hpp"
#include "device.hpp"
#include "tms9901.hpp"

#define WP  WorkspacePtr
#define PC  ProgramCounter
#define ST  Status

DBG_REGISTER ( __FILE__ );

static bool    isFetch;
static int     runFlag;
static int     stopFlag;
static UINT16  curOpCode;

extern "C" {

    UINT8   CpuMemory [ 0x10000 ];
    UINT8   MemFlags  [ 0x10000 ];
    UINT16  InterruptFlag;
    UINT16  WorkspacePtr;
    UINT16  ProgramCounter;
    UINT16  Status;
    UINT32  InstructionCounter;
    UINT32  ClockCycleCounter;

    bool Step ();
    void Run ();
    void Stop ();
    bool IsRunning ();
    void ContextSwitch ( UINT16 address );

}

extern cTMS9901 *pic;

extern "C" void   *CRU_Object;
extern "C" sLookUp LookUp [ 16 ];
extern "C" UINT16  parity [ 256 ];

extern "C" void InvalidOpcode ();
extern "C" int (*TimerHook) ();
extern "C" UINT8 CallTrapB ( bool read, ADDRESS address, UINT8 value );
extern "C" UINT16 CallTrapW ( bool read, bool isFetch, ADDRESS address, UINT16 value );
extern "C" UINT16 ReadCRU ( void *, int, int );
extern "C" void WriteCRU ( void *, int, int, UINT16 );

static UINT16 ReadMemoryW ( UINT16 address )
{
    UINT8 flags = MemFlags [ address ] | ( MemFlags [ address + 1 ] & MEMFLG_DEBUG );

    // This is a hack to work around the scratch pad RAM memory addressing
    UINT16 offset = ( flags & MEMFLG_SCRATCHPAD ) ? address | 0x8300 : address;

    UINT16 retVal = ( UINT16 ) (( CpuMemory [ offset ] << 8 ) | CpuMemory [ offset + 1 ] );

    if ( flags & ( MEMFLG_TRAP_READ | MEMFLG_READ | MEMFLG_FETCH | MEMFLG_8BIT )) {

        // Add 4 clock cycles if we're accessing 8-bit memory
        ClockCycleCounter += flags & MEMFLG_8BIT;

        if (( flags & ( MEMFLG_TRAP_READ | MEMFLG_READ )) || (( flags & MEMFLG_FETCH ) && ( isFetch == true ))) {
            retVal = CallTrapW ( true, isFetch, offset, retVal );
        }
    }

    return retVal;
}

static UINT8 ReadMemoryB ( UINT16 address )
{
    UINT8 flags = MemFlags [ address ];

    // This is a hack to work around the scratch pad RAM memory addressing
    UINT16 offset = ( flags & MEMFLG_SCRATCHPAD ) ? address | 0x8300 : address;

    UINT8 retVal = CpuMemory [ offset ];

    if ( flags & ( MEMFLG_TRAP_READ | MEMFLG_READ | MEMFLG_8BIT )) {

        // Add 4 clock cycles if we're accessing 8-bit memory
        ClockCycleCounter += flags & MEMFLG_8BIT;

        if ( flags & ( MEMFLG_TRAP_READ | MEMFLG_READ )) {
            retVal = ( UINT8 ) CallTrapB ( true, offset, retVal );
        }
    }

    return retVal;
}

static void WriteMemoryW ( UINT16 address, UINT16 value, int penalty = 4 )
{
    UINT8 flags = MemFlags [ address ] | ( MemFlags [ address + 1 ] & MEMFLG_DEBUG );

    // This is a hack to work around the scratch pad RAM memory addressing
    UINT16 offset = ( flags & MEMFLG_SCRATCHPAD ) ? address | 0x8300 : address;

    if ( flags & ( MEMFLG_TRAP_WRITE | MEMFLG_WRITE | MEMFLG_8BIT | MEMFLG_ROM )) {

        if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4 + penalty;

        if ( flags & ( MEMFLG_TRAP_WRITE | MEMFLG_WRITE )) {
            value = CallTrapW ( false, false, address, value );
        }

        if ( flags & MEMFLG_ROM ) return;
    }

    UINT8 *ptr = &CpuMemory [ offset ];
    ptr [0] = ( UINT8 ) ( value >> 8 );
    ptr [1] = ( UINT8 ) value;
}

static void WriteMemoryB ( UINT16 address, UINT8 value, int penalty = 4 )
{
    UINT8 flags = MemFlags [ address ];

    // This is a hack to work around the scratch pad RAM memory addressing
    UINT16 offset = ( flags & MEMFLG_SCRATCHPAD ) ? address | 0x8300 : address;

    if ( flags & ( MEMFLG_TRAP_WRITE | MEMFLG_WRITE | MEMFLG_8BIT | MEMFLG_ROM )) {

        if ( flags & MEMFLG_8BIT ) ClockCycleCounter += 4 + penalty;

        if ( flags & ( MEMFLG_TRAP_WRITE | MEMFLG_WRITE )) {
            value = ( UINT8 ) CallTrapB ( false, offset, value );
        }

        if ( flags & MEMFLG_ROM ) return;
    }

    CpuMemory [ offset ] = value;
}

static UINT16 Fetch ()
{
    isFetch = true;
    UINT16 retVal = ReadMemoryW ( PC );
    isFetch = false;
    PC += 2;
    return retVal;
}

static void _ExecuteInstruction ( UINT16 opCode )
{
    sLookUp *lookup = &LookUp [ opCode >> 12 ];
    sOpCode *op = lookup->opCode;
    int retries = lookup->size;

    do {
        if (( opCode & op->mask ) == op->opCode ) {
            ClockCycleCounter += op->clocks;
            ((void(*)()) op->function ) ();
            op->count++;
            return;
        }
        op++;
    } while ( retries-- );

    // Add minimum clock cycle count
    ClockCycleCounter += 6;

    InvalidOpcode ();
}

static void ExecuteInstruction ()
{
    curOpCode = Fetch ();
    _ExecuteInstruction ( curOpCode );
    InstructionCounter++;

    if ((( char ) InstructionCounter == 0 ) && ( TimerHook != NULL )) {
        TimerHook ();
    }
}

//             T   Clk Acc
// Rx          00   0   0          Register
// *Rx         01   4   1          Register Indirect
// *Rx+        11   6   2 (byte)   Auto-increment
// *Rx+        11   8   2 (word)   Auto-increment
// @>xxxx      10   8   1          Symbolic Memory
// @>xxxx(Rx)  10   8   2          Indexed Memory
//

static UINT16 GetAddress ( UINT16 opCode, int size )
{
    UINT16 address = 0;
    int reg = opCode & 0x0F;

    switch ( opCode & 0x0030 ) {
        case 0x0000 : address = ( UINT16 ) ( WP + 2 * reg );
                      break;
        case 0x0010 : address = ReadMemoryW ( WP + 2 * reg );
                      ClockCycleCounter += 4;
                      break;
        case 0x0030 : address = ReadMemoryW ( WP + 2 * reg );
                      WriteMemoryW ( WP + 2 * reg, ( UINT16 ) ( address + size ), 0 );
                      ClockCycleCounter += 4 + 2 * size;
                      break;
        case 0x0020 : if ( reg ) address = ReadMemoryW ( WP + 2 * reg );
                      address += Fetch ();
                      ClockCycleCounter += 8;
                      break;
    }

    if ( size != 1 ) {
        address &= 0xFFFE;
    }

    return address;
}

static bool CheckInterrupt ()
{
    // Tell the PIC to update it's timer and turn off old interrupts
    pic->UpdateTimer ( ClockCycleCounter );

    // Look for pending unmasked interrupts
    UINT16 mask = ( UINT16 ) (( 2 << ( ST & 0x0F )) - 1 );
    UINT16 pending = InterruptFlag & mask;

    if ( pending == 0 ) return false;

    // Find the highest priority interrupt
    int level = 0;
    mask = 1;
    while (( pending & mask ) == 0 ) {
        level++;
        mask <<= 1;
    }

    ContextSwitch ( level * 4 );

    if ( level != 0 ) {
        ST &= 0xFFF0;
        ST |= level - 1;
    }

    return true;
}

bool Step ()
{
    runFlag++;

    if ( CheckInterrupt () == true ) return false;

    ExecuteInstruction ();

    runFlag--;
    if ( stopFlag ) {
        stopFlag--;
        return true;
    }

    return false;
}

void Run ()
{
    runFlag++;

    do {
        CheckInterrupt ();
        ExecuteInstruction ();
    } while ( stopFlag == 0 );

    stopFlag--;
    runFlag--;
}

void Stop ()
{
    stopFlag++;
}

bool IsRunning ()
{
    return ( runFlag != 0 ) ? true : false;
}

void ContextSwitch ( UINT16 address )
{
    UINT16 newWP = ReadMemoryW ( address );
    UINT16 newPC = ReadMemoryW ( address + 2 );

    UINT16 oldWP = WP;
    UINT16 oldPC = PC;
    WP = newWP;
    PC = newPC;

    WriteMemoryW ( WP + 2 * 13, oldWP, 0 );
    WriteMemoryW ( WP + 2 * 14, oldPC, 0 );
    WriteMemoryW ( WP + 2 * 15, ST,    0 );
}

static void SetFlags_LAE ( UINT16 val )
{
    if (( short ) val > 0 ) {
        ST |= TMS_LOGICAL | TMS_ARITHMETIC;
    } else if (( short ) val < 0 ) {
        ST |= TMS_LOGICAL;
    } else {
        ST |= TMS_EQUAL;
    }
}

static void SetFlags_LAE ( UINT16 val1, UINT16 val2 )
{
    if ( val1 == val2 ) {
        ST |= TMS_EQUAL;
    } else {
        if (( short ) val1 > ( short ) val2 ) {
            ST |= TMS_ARITHMETIC;
        }
        if ( val1 > val2 ) {
            ST |= TMS_LOGICAL;
        }
    }
}

static void SetFlags_difW ( UINT16 val1, UINT16 val2, UINT32 res )
{
    if ( ! ( res & 0x00010000 )) ST |= TMS_CARRY;
    if (( val1 ^ val2 ) & ( val1 ^ res ) & 0x8000 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( UINT16 ) res );
}

static void SetFlags_difB ( UINT8 val1, UINT8 val2, UINT32 res )
{
    if ( ! ( res & 0x0100 )) ST |= TMS_CARRY;
    if (( val1 ^ val2 ) & ( val1 ^ res ) & 0x80 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( char ) res );
    ST |= parity [ ( UINT8 ) res ];
}

static void SetFlags_sumW ( UINT16 val1, UINT16 val2, UINT32 res )
{
    if ( res & 0x00010000 ) ST |= TMS_CARRY;
    if (( res ^ val1 ) & ( res ^ val2 ) & 0x8000 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( UINT16 ) res );
}

static void SetFlags_sumB ( UINT8 val1, UINT8 val2, UINT32 res )
{
    if ( res & 0x0100 ) ST |= TMS_CARRY;
    if (( res ^ val1 ) & ( res ^ val2 ) & 0x80 ) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( char ) res );
    ST |= parity [ ( UINT8 ) res ];
}

//-----------------------------------------------------------------------------
//   LI		Format: VIII	Op-code: 0x0200		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_LI ()
{
    UINT16 value = Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), value, 0 );
}

//-----------------------------------------------------------------------------
//   AI		Format: VIII	Op-code: 0x0220		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_AI ()
{
    int reg = curOpCode & 0x0F;

    UINT32 src = ReadMemoryW ( WP + 2 * reg );
    UINT32 dst = Fetch ();
    UINT32 sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( UINT16 ) src, ( UINT16 ) dst, sum );

    WriteMemoryW ( WP + 2 * reg, ( UINT16 ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   ANDI	Format: VIII	Op-code: 0x0240		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_ANDI ()
{
    UINT16 reg = ( UINT16 ) ( curOpCode & 0x000F );
    UINT16 value = ReadMemoryW ( WP + 2 * reg );
    value &= Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   ORI	Format: VIII	Op-code: 0x0260		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_ORI ()
{
    UINT16 reg = ( UINT16 ) ( curOpCode & 0x000F );
    UINT16 value = ReadMemoryW ( WP + 2 * reg );
    value |= Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   CI		Format: VIII	Op-code: 0x0280		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_CI ()
{
    UINT16 src = ReadMemoryW ( WP + 2 * ( curOpCode & 0x000F ));
    UINT16 dst = Fetch ();

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src, dst );
}

//-----------------------------------------------------------------------------
//   STWP	Format: VIII	Op-code: 0x02A0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_STWP ()
{
    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), WP, 0 );
}

//-----------------------------------------------------------------------------
//   STST	Format: VIII	Op-code: 0x02C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_STST ()
{
    WriteMemoryW ( WP + 2 * ( curOpCode & 0x000F ), ST );
}

//-----------------------------------------------------------------------------
//   LWPI	Format: VIII	Op-code: 0x02E0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LWPI ()
{
    WP = Fetch ();
}

//-----------------------------------------------------------------------------
//   LIMI	Format: VIII	Op-code: 0x0300		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LIMI ()
{
    ST = ( UINT16 ) (( ST & 0xFFF0 ) | ( Fetch () & 0x0F ));
}

//-----------------------------------------------------------------------------
//   IDLE	Format: VII	Op-code: 0x0340		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_IDLE ()
{
    for ( EVER ) {
        if ( CheckInterrupt () == true ) return;
        TimerHook ();
        ClockCycleCounter += 4;
    }
}

//-----------------------------------------------------------------------------
//   RSET	Format: VII	Op-code: 0x0360		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_RSET ()
{
    // Set the interrupt mask to 0
    ST &= 0xFFF0;
}

//-----------------------------------------------------------------------------
//   RTWP	Format: VII	Op-code: 0x0380		Status: L A E C O P X
//-----------------------------------------------------------------------------
void opcode_RTWP ()
{
    ST = ReadMemoryW ( WP + 2 * 15 );
    PC = ReadMemoryW ( WP + 2 * 14 );
    WP = ReadMemoryW ( WP + 2 * 13 );
}

//-----------------------------------------------------------------------------
//   CKON	Format: VII	Op-code: 0x03A0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CKON ()
{
}

//-----------------------------------------------------------------------------
//   CKOF	Format: VII	Op-code: 0x03C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CKOF ()
{
}

//-----------------------------------------------------------------------------
//   LREX	Format: VII	Op-code: 0x03E0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_LREX ()
{
}

//-----------------------------------------------------------------------------
//   BLWP	Format: VI	Op-code: 0x0400		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_BLWP ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    ContextSwitch ( address );
}

//-----------------------------------------------------------------------------
//   B		Format: VI	Op-code: 0x0440		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_B ()
{
    PC = GetAddress ( curOpCode, 2 );
}

//-----------------------------------------------------------------------------
//   X		Format: VI	Op-code: 0x0480		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_X ()
{
    curOpCode = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    _ExecuteInstruction ( curOpCode );
}

//-----------------------------------------------------------------------------
//   CLR	Format: VI	Op-code: 0x04C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_CLR ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( address, ( UINT16 ) 0, 4 );
}

//-----------------------------------------------------------------------------
//   NEG	Format: VI	Op-code: 0x0500		Status: L A E - O - -
//-----------------------------------------------------------------------------
void opcode_NEG ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( address );

    UINT32 dst = 0 - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW );
    SetFlags_LAE (( UINT16 ) dst );
    if (( src ^ dst ) & 0x8000 ) ST |= TMS_OVERFLOW;

    WriteMemoryW ( address, ( UINT16 ) dst, 0 );
}

//-----------------------------------------------------------------------------
//   INV	Format: VI	Op-code: 0x0540		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_INV ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT16 value = ~ ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( address, value, 0 );
}

//-----------------------------------------------------------------------------
//   INC	Format: VI	Op-code: 0x0580		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_INC ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( address );

    UINT32 sum = src + 1;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( UINT16 ) src, 1, sum );

    WriteMemoryW ( address, ( UINT16 ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   INCT	Format: VI	Op-code: 0x05C0		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_INCT ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( address );

    UINT32 sum = src + 2;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( UINT16 ) src, 2, sum );

    WriteMemoryW ( address, ( UINT16 ) sum, 0 );
}

//-----------------------------------------------------------------------------
//   DEC	Format: VI	Op-code: 0x0600		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_DEC ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( address );

    UINT32 dif = src - 1;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( UINT16 ) src, 1, dif );

    WriteMemoryW ( address, ( UINT16 ) dif, 0 );
}

//-----------------------------------------------------------------------------
//   DECT	Format: VI	Op-code: 0x0640		Status: L A E C O - -
    //-----------------------------------------------------------------------------
void opcode_DECT ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( address );

    UINT32 dif = src - 2;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( UINT16 ) src, 2, dif );

    WriteMemoryW ( address, ( UINT16 ) dif, 0 );
}

//-----------------------------------------------------------------------------
//   BL		Format: VI	Op-code: 0x0680		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_BL ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( WP + 2 * 11, PC, 4 );
    PC = address;
}

//-----------------------------------------------------------------------------
//   SWPB	Format: VI	Op-code: 0x06C0		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SWPB ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT16 value = ReadMemoryW ( address );
    value = ( UINT16 ) (( value << 8 ) | ( value >> 8 ));
    WriteMemoryW ( address, ( UINT16 ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SETO	Format: VI	Op-code: 0x0700		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SETO ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    WriteMemoryW ( address, ( UINT16 ) -1, 4 );
}

//-----------------------------------------------------------------------------
//   ABS	Format: VI	Op-code: 0x0740		Status: L A E - O - -
//-----------------------------------------------------------------------------
void opcode_ABS ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT16 dst = ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW );
    SetFlags_LAE ( dst );

    if ( dst & 0x8000 ) {
        ClockCycleCounter += 2;
        WriteMemoryW ( address, -dst, 0 );
        ST |= TMS_OVERFLOW;
    }
}

//-----------------------------------------------------------------------------
//   SRA	Format: V	Op-code: 0x0800		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRA ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY );

    short value = ( short ) ((( short ) ReadMemoryW ( WP + 2 * reg )) >> --count );
    if ( value & 1 ) ST |= TMS_CARRY;
    value >>= 1;
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, ( UINT16 ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SRL	Format: V	Op-code: 0x0900		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRL ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY );

    UINT16 value = ( UINT16 ) ( ReadMemoryW ( WP + 2 * reg ) >> --count );
    if ( value & 1 ) ST |= TMS_CARRY;
    value >>= 1;
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   SLA	Format: V	Op-code: 0x0A00		Status: L A E C O - -
//
// Comments: The overflow bit is set if the sign changes during the shift
//-----------------------------------------------------------------------------
void opcode_SLA ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );

    long value = ReadMemoryW ( WP + 2 * reg ) << count;

    UINT32 mask = (( UINT16 ) -1 << count ) & 0xFFFF8000;
    int bits = value & mask;

    if ( value & 0x00010000 ) ST |= TMS_CARRY;
    if ( bits && ( bits ^ mask )) ST |= TMS_OVERFLOW;
    SetFlags_LAE (( UINT16 ) value );

    WriteMemoryW ( WP + 2 * reg, ( UINT16 ) value, 0 );
}

//-----------------------------------------------------------------------------
//   SRC	Format: V	Op-code: 0x0B00		Status: L A E C - - -
//-----------------------------------------------------------------------------
void opcode_SRC ()
{
    int reg = curOpCode & 0x000F;
    int count = ( curOpCode >> 4 ) & 0x000F;
    if ( count == 0 ) {
        ClockCycleCounter += 8;
        count = ReadMemoryW ( WP + 2 * 0 ) & 0x000F;
        if ( count == 0 ) count = 16;
    }

    ClockCycleCounter += 2 * count;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );

    int value = ReadMemoryW ( WP + 2 * reg );
    value = (( value << 16 ) | value ) >> count;
    if ( value & 0x8000 ) ST |= TMS_CARRY;
    SetFlags_LAE (( UINT16 ) value );

    WriteMemoryW ( WP + 2 * reg, ( UINT16 ) value, 0 );
}

//-----------------------------------------------------------------------------
//   JMP	Format: II	Op-code: 0x1000		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JMP ()
{
    ClockCycleCounter += 2;
    PC += 2 * ( char ) curOpCode;
}

//-----------------------------------------------------------------------------
//   JLT	Format: II	Op-code: 0x1100		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JLT ()
{
    if ( ! ( ST & ( TMS_ARITHMETIC | TMS_EQUAL ))) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JLE	Format: II	Op-code: 0x1200		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JLE ()
{
    if (( ! ( ST & TMS_LOGICAL )) | ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JEQ	Format: II	Op-code: 0x1300		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JEQ ()
{
    if ( ST & TMS_EQUAL ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JHE	Format: II	Op-code: 0x1400		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JHE ()
{
    if ( ST & ( TMS_LOGICAL | TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JGT	Format: II	Op-code: 0x1500		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JGT ()
{
    if ( ST & TMS_ARITHMETIC ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNE	Format: II	Op-code: 0x1600		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNE ()
{
    if ( ! ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNC	Format: II	Op-code: 0x1700		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNC ()
{
    if ( ! ( ST & TMS_CARRY )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JOC	Format: II	Op-code: 0x1800		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JOC ()
{
    if ( ST & TMS_CARRY ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JNO	Format: II	Op-code: 0x1900		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JNO ()
{
    if ( ! ( ST & TMS_OVERFLOW )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JL		Format: II	Op-code: 0x1A00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JL ()
{
    if ( ! ( ST & ( TMS_LOGICAL | TMS_EQUAL ))) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JH		Format: II	Op-code: 0x1B00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JH ()
{
    if (( ST & TMS_LOGICAL ) && ! ( ST & TMS_EQUAL )) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   JOP	Format: II	Op-code: 0x1C00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_JOP ()
{
    if ( ST & TMS_PARITY ) opcode_JMP ();
}

//-----------------------------------------------------------------------------
//   SBO	Format: II	Op-code: 0x1D00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SBO ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    ClockCycleCounter += 2;
    WriteCRU ( CRU_Object, cru, 1, 1 );
}

//-----------------------------------------------------------------------------
//   SBZ	Format: II	Op-code: 0x1E00		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_SBZ ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    ClockCycleCounter += 2;
    WriteCRU ( CRU_Object, cru, 1, 0 );
}

//-----------------------------------------------------------------------------
//   TB		Format: II	Op-code: 0x1F00		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_TB ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) + ( curOpCode & 0x00FF );
    ClockCycleCounter += 2;
    if ( ReadCRU ( CRU_Object, cru, 1 ) & 1 ) ST |= TMS_EQUAL;
    else ST &= ~ TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   COC	Format: III	Op-code: 0x2000		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_COC ()
{
    UINT16 src = ReadMemoryW ( WP + 2 * (( curOpCode >> 6 ) & 0x000F ));
    UINT16 dst = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    if (( src & dst ) == dst ) ST |= TMS_EQUAL;
    else ST &= ~ TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   CZC	Format: III	Op-code: 0x2400		Status: - - E - - - -
//-----------------------------------------------------------------------------
void opcode_CZC ()
{
    UINT16 src = ReadMemoryW ( WP + 2 * (( curOpCode >> 6 ) & 0x000F ));
    UINT16 dst = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    if (( ~ src & dst ) == dst ) ST |= TMS_EQUAL;
    else ST &= ~ TMS_EQUAL;
}

//-----------------------------------------------------------------------------
//   XOR	Format: III	Op-code: 0x2800		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_XOR ()
{
    int reg = ( curOpCode >> 6 ) & 0x000F;
    UINT16 address = GetAddress ( curOpCode, 2 );
    UINT16 value = ReadMemoryW ( WP + 2 * reg );
    value ^= ReadMemoryW ( address );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( value );

    WriteMemoryW ( WP + 2 * reg, value, 0 );
}

//-----------------------------------------------------------------------------
//   XOP	Format: IX	Op-code: 0x2C00		Status: - - - - - - X
//-----------------------------------------------------------------------------
void opcode_XOP ()
{
    UINT16 address = GetAddress ( curOpCode, 2 );
    int level = (( curOpCode >> 4 ) & 0x003C ) + 64;
    ContextSwitch ( level );
    WriteMemoryW ( WP + 2 * 11, address );
    ST |= TMS_XOP;
}

//-----------------------------------------------------------------------------
//   LDCR	Format: IV	Op-code: 0x3000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_LDCR ()
{
    UINT16 value;
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) & 0x0FFF;
    int count = ( curOpCode >> 6 ) & 0x000F;
    if ( count == 0 ) count = 16;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW | TMS_PARITY );

    ClockCycleCounter += 2 * count;
    if ( count < 9 ) {
        UINT16 address = GetAddress ( curOpCode, 1 );
        value = ReadMemoryB ( address );
        ST |= parity [ ( UINT8 ) value ];
        SetFlags_LAE (( char ) value );
    } else {
        UINT16 address = GetAddress ( curOpCode, 2 );
        value = ReadMemoryW ( address );
        SetFlags_LAE ( value );
    }

    WriteCRU ( CRU_Object, cru, count, value );
}

//-----------------------------------------------------------------------------
//   STCR	Format: IV	Op-code: 0x3400		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_STCR ()
{
    int cru = ( ReadMemoryW ( WP + 2 * 12 ) >> 1 ) & 0x0FFF;
    int count = ( curOpCode >> 6 ) & 0x000F;
    if ( count == 0 ) count = 16;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_OVERFLOW | TMS_PARITY );

    ClockCycleCounter += 2 * count;
    UINT16 value = ReadCRU ( CRU_Object, cru, count );
    if ( count < 9 ) {
        ST |= parity [ ( UINT8 ) value ];
        SetFlags_LAE (( char ) value );
        UINT16 address = GetAddress ( curOpCode, 1 );
        WriteMemoryB ( address, ( UINT8 ) value );
    } else {
        ClockCycleCounter += 58 - 42;
        SetFlags_LAE ( value );
        UINT16 address = GetAddress ( curOpCode, 2 );
        WriteMemoryW ( address, value );
    }
}

//-----------------------------------------------------------------------------
//   MPY	Format: IX	Op-code: 0x3800		Status: - - - - - - -
//-----------------------------------------------------------------------------
void opcode_MPY ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress (( curOpCode >> 6 ) & 0x0F, 2 );
    UINT32 dst = ReadMemoryW ( dstAddress );

    dst *= src;

    WriteMemoryW ( dstAddress, ( UINT16 ) ( dst >> 16 ));
    WriteMemoryW ( dstAddress + 2, ( UINT16 ) dst );
}

//-----------------------------------------------------------------------------
//   DIV	Format: IX	Op-code: 0x3C00		Status: - - - - O - -
//-----------------------------------------------------------------------------
void opcode_DIV ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress (( curOpCode >> 6 ) & 0x0F, 2 );
    UINT32 dst = ReadMemoryW ( dstAddress );

    if ( dst < src ) {
        ST &= ~ TMS_OVERFLOW;
        dst = ( dst << 16 ) | ReadMemoryW ( dstAddress + 2 );
        WriteMemoryW ( dstAddress, ( UINT16 ) ( dst / src ));
        WriteMemoryW ( dstAddress + 2, ( UINT16 ) ( dst % src ));
        ClockCycleCounter += ( 92 + 124 ) / 2 - 16;
    } else {
        ST |= TMS_OVERFLOW;
    }
}

//-----------------------------------------------------------------------------
//   SZC	Format: I	Op-code: 0x4000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_SZC ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT16 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 2 );
    UINT16 dst = ReadMemoryW ( dstAddress );

    src = ~ src & dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   SZCB	Format: I	Op-code: 0x5000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_SZCB ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 1 );
    UINT8  src = ReadMemoryB ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UINT8  dst = ReadMemoryB ( dstAddress );

    src = ~ src & dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   S		Format: I	Op-code: 0x6000		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_S ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 2 );
    UINT32 dst = ReadMemoryW ( dstAddress );

    UINT32 sum = dst - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_difW (( UINT16 ) src, ( UINT16 ) dst, sum );

    WriteMemoryW ( dstAddress, ( UINT16 ) sum );
}

//-----------------------------------------------------------------------------
//   SB		Format: I	Op-code: 0x7000		Status: L A E C O P -
//-----------------------------------------------------------------------------
void opcode_SB ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 1 );
    UINT32 src = ReadMemoryB ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UINT32 dst = ReadMemoryB ( dstAddress );

    UINT32 sum = dst - src;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW | TMS_PARITY );
    SetFlags_difB (( UINT8 ) src, ( UINT8 ) dst, sum );

    WriteMemoryB ( dstAddress, ( UINT8 ) sum );
}

//-----------------------------------------------------------------------------
//   C		Format: I	Op-code: 0x8000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_C ()
{
    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );

    UINT16 src = ReadMemoryW ( GetAddress ( curOpCode, 2 ));
    UINT16 dst = ReadMemoryW ( GetAddress ( curOpCode >> 6 , 2 ));

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src, dst );
}

//-----------------------------------------------------------------------------
//   CB		Format: I	Op-code: 0x9000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_CB ()
{
    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );

    UINT8 src = ReadMemoryB ( GetAddress ( curOpCode, 1 ));
    UINT8 dst = ReadMemoryB ( GetAddress ( curOpCode >> 6 , 1 ));

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src, ( char ) dst );
}

//-----------------------------------------------------------------------------
//   A		Format: I	Op-code: 0xA000		Status: L A E C O - -
//-----------------------------------------------------------------------------
void opcode_A ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT32 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 2 );
    UINT32 dst = ReadMemoryW ( dstAddress );

    UINT32 sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW );
    SetFlags_sumW (( UINT16 ) src, ( UINT16 ) dst, sum );

    WriteMemoryW ( dstAddress, ( UINT16 ) sum );
}

//-----------------------------------------------------------------------------
//   AB		Format: I	Op-code: 0xB000		Status: L A E C O P -
//-----------------------------------------------------------------------------
void opcode_AB ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 1 );
    UINT32 src = ReadMemoryB ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UINT32 dst = ReadMemoryB ( dstAddress );

    UINT32 sum = src + dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_CARRY | TMS_OVERFLOW | TMS_PARITY );
    ST |= parity [ ( UINT8 ) sum ];
    SetFlags_sumB (( UINT8 ) src, ( UINT8 ) dst, sum );

    WriteMemoryB ( dstAddress, ( UINT8 ) sum );
}

//-----------------------------------------------------------------------------
//   MOV	Format: I	Op-code: 0xC000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_MOV ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT16 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 2 );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src, 4 );
}

//-----------------------------------------------------------------------------
//   MOVB	Format: I	Op-code: 0xD000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_MOVB ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 1 );
    UINT8  src = ReadMemoryB ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 1 );

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src, 4 );
}

//-----------------------------------------------------------------------------
//   SOC	Format: I	Op-code: 0xE000		Status: L A E - - - -
//-----------------------------------------------------------------------------
void opcode_SOC ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 2 );
    UINT16 src = ReadMemoryW ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 2 );
    UINT16 dst = ReadMemoryW ( dstAddress );

    src = src | dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL );
    SetFlags_LAE ( src );

    WriteMemoryW ( dstAddress, src );
}

//-----------------------------------------------------------------------------
//   SOCB	Format: I	Op-code: 0xF000		Status: L A E - - P -
//-----------------------------------------------------------------------------
void opcode_SOCB ()
{
    UINT16 srcAddress = GetAddress ( curOpCode, 1 );
    UINT8  src = ReadMemoryB ( srcAddress );
    UINT16 dstAddress = GetAddress ( curOpCode >> 6, 1 );
    UINT8  dst = ReadMemoryB ( dstAddress );

    src = src | dst;

    ST &= ~ ( TMS_LOGICAL | TMS_ARITHMETIC | TMS_EQUAL | TMS_PARITY );
    ST |= parity [ src ];
    SetFlags_LAE (( char ) src );

    WriteMemoryB ( dstAddress, src );
}
