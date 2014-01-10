//----------------------------------------------------------------------------
//
// File:        opcodes.hpp
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

extern "C" {

    void opcode_A    ();
    void opcode_AB   ();
    void opcode_ABS  ();
    void opcode_AI   ();
    void opcode_ANDI ();
    void opcode_B    ();
    void opcode_BL   ();
    void opcode_BLWP ();
    void opcode_C    ();
    void opcode_CB   ();
    void opcode_CI   ();
    void opcode_CKOF ();
    void opcode_CKON ();
    void opcode_CLR  ();
    void opcode_COC  ();
    void opcode_CZC  ();
    void opcode_DEC  ();
    void opcode_DECT ();
    void opcode_DIV  ();
    void opcode_IDLE ();
    void opcode_INC  ();
    void opcode_INCT ();
    void opcode_INV  ();
    void opcode_JEQ  ();
    void opcode_JGT  ();
    void opcode_JH   ();
    void opcode_JHE  ();
    void opcode_JL   ();
    void opcode_JLE  ();
    void opcode_JLT  ();
    void opcode_JMP  ();
    void opcode_JNC  ();
    void opcode_JNE  ();
    void opcode_JNO  ();
    void opcode_JOC  ();
    void opcode_JOP  ();
    void opcode_LDCR ();
    void opcode_LI   ();
    void opcode_LIMI ();
    void opcode_LREX ();
    void opcode_LWPI ();
    void opcode_MOV  ();
    void opcode_MOVB ();
    void opcode_MPY  ();
    void opcode_NEG  ();
    void opcode_ORI  ();
    void opcode_RSET ();
    void opcode_RTWP ();
    void opcode_S    ();
    void opcode_SB   ();
    void opcode_SBO  ();
    void opcode_SBZ  ();
    void opcode_SETO ();
    void opcode_SLA  ();
    void opcode_SOC  ();
    void opcode_SOCB ();
    void opcode_SRA  ();
    void opcode_SRC  ();
    void opcode_SRL  ();
    void opcode_STCR ();
    void opcode_STST ();
    void opcode_STWP ();
    void opcode_SWPB ();
    void opcode_SZC  ();
    void opcode_SZCB ();
    void opcode_TB   ();
    void opcode_X    ();
    void opcode_XOP  ();
    void opcode_XOR  ();

}
