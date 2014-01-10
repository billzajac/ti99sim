//----------------------------------------------------------------------------
//
// File:        tms9901.cpp
// Date:        18-Dec-2001
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2001-2004 Marc Rousseau, All Rights Reserved.
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
#include <string.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "device.hpp"
#include "tms9901.hpp"
#include "ti994a.hpp"

DBG_REGISTER ( __FILE__ );

#define SET_MASK        0xAA

cTMS9901 *pic;

cTMS9901::cTMS9901 ( cTMS9900 *pCPU ) :
    cDevice ( NULL ),
    m_TimerActive ( false ),
    m_ReadRegister ( 0 ),
    m_Decrementer ( 0 ),
    m_ClockRegister ( 0 ),
    m_PinState (),
    m_InterruptRequested ( 0 ),
    m_ActiveInterrupts ( 0 ),
    m_LastDelta ( 0 ),
    m_DecrementClock ( 0 ),
    m_LastClockCycle ( 0 ),
    m_CapsLock ( false ),
    m_ColumnSelect ( 0 ),
    m_StateTable (),
    m_KSLinkTable (),
    m_Joystick ()
{
    FUNCTION_ENTRY ( this, "cTMS9901 ctor", true );

    pic = this;

    SetCPU ( pCPU );

    m_CRU = 0;

    // Mark pins P0-P16 as input/interrupt pins
    for ( int i = 16; i < 32; i++ ) {
        m_PinState [i][1] = -1;
    }
}

cTMS9901::~cTMS9901 ()
{
    FUNCTION_ENTRY ( this, "cTMS9901 dtor", true );
}

void cTMS9901::WriteCRU ( ADDRESS address, int data )
{
    FUNCTION_ENTRY ( this, "cTMS9901::WriteCRU", false );

    DBG_ASSERT ( data <= 1 );

    // Address lines A4-A10 are not decoded - alias the address space
    address &= 0x3F;

    if ( address == 0 ) {
        m_PinState [0][1] = data;
        if ( data == 1 ) {
            DBG_STATUS ( "Timer mode On" );
            m_ReadRegister = m_Decrementer;
        } else {
            DBG_STATUS ( "I/O mode On" );
            if ( m_ClockRegister != 0 ) {
                m_TimerActive = true;
                DBG_TRACE ( "Timer: " << hex << ( UINT16 ) m_ClockRegister );
            }
            m_Decrementer    = m_ClockRegister;
            m_DecrementClock = m_LastClockCycle;
            m_LastDelta      = 0;
        }
    } else {
        if ( m_PinState [0][1] == 1 ) {
            // We're in timer mode
            if (( address >= 1 ) && ( address <= 14 )) {
                int mask = 1 << ( address - 1 );
                m_ClockRegister &= ~mask;
                m_ClockRegister |= mask;
                m_Decrementer = m_ClockRegister;
            } else if ( address == 15 ) {
                SoftwareReset ();
            }
        } else {
            // We're in I/O mode
            m_PinState [address][1] = ( char ) data;

            if (( address >= 18 ) && ( address <= 20 )) {
                int shift = address - 18;
                m_ColumnSelect &= ~ ( 1 << shift );
                m_ColumnSelect |= data << shift;
            } else if ( address == 21 ) {
                m_CapsLock = ( data != 0 ) ? true : false;
            }
        }
    }
}

/*
    >00  0 = Internal 9901 Control   1 = Clock Control
    >01  Set by an external Interrupt
    >02  Set by TMS9918A on Vertical Retrace Interrupt
    >03  Set by Clock Interrupt for Cassette read/write routines
    >0C  Reserved - High Level
    >16  Cassette CS1 motor control On/Off
    >17  Cassette CS2 motor control On/Off
    >18  Audio Gate enable/disable
    >19  Cassette Tape Out
    >1B  Cassette Tape In
*/

int cTMS9901::ReadCRU ( unsigned short address )
{
    FUNCTION_ENTRY ( this, "cTMS9901::ReadCRU", false );

    // TI Keyboard Matrix
    static UINT16 Keys [8][6] = {
        { VK_EQUALS, VK_PERIOD, VK_COMMA, VK_M,   VK_N,   VK_DIVIDE    },
        { VK_SPACE,  VK_L,      VK_K,     VK_J,   VK_H,   VK_SEMICOLON },
        { VK_ENTER,  VK_O,      VK_I,     VK_U,   VK_Y,   VK_P         },
        { 0 ,        VK_9,      VK_8,     VK_7,   VK_6,   VK_0         },
        { VK_FCTN,   VK_2,      VK_3,     VK_4,   VK_5,   VK_1         },
        { VK_SHIFT,  VK_S,      VK_D,     VK_F,   VK_G,   VK_A         },
        { VK_CTRL,   VK_W,      VK_E,     VK_R,   VK_T,   VK_Q         },
        { 0,         VK_X,      VK_C,     VK_V,   VK_B,   VK_Z         }
    };

    // Address lines A4-A10 are not decoded - alias the address space
    address &= 0x3F;

    int retVal = 1;

    if ( m_PinState [0][1] == 1 ) {
        // We're in timer mode
        if ( address == 0 ) {
            // Mode
            retVal = 1;
        } else if (( address >= 1 ) && ( address <= 14 )) {
            // ReadRegister
            int mask = 1 << ( address - 1 );
            retVal = ( m_ReadRegister & mask ) ? 1 : 0;
        } else if ( address == 15 ) {
            // INTREQ
            retVal = ( m_InterruptRequested > 0 ) ? 1 : 0;
        }
    } else {
        // We're in I/O mode

        // Adjust for the aliased pins
        if (( address >= 23 ) && ( address <= 31 )) {
            address = 38 - address;
        }

        if ( address == 0 ) {
            // Mode
            retVal = 0;
        } else if (( address >= 1 ) && ( address <= 2 )) {
            // Interrupt status INT1-INT2
            if ( m_PinState [address][0] != 0 ) retVal = 0;
        } else if (( address >= 3 ) && ( address <= 10 )) {

            if (( m_CapsLock == false ) && ( address == 7 )) {
                if ( m_StateTable [ VK_CAPSLOCK ] != 0 ) retVal = 0;
            }

            switch ( m_ColumnSelect ) {
                case 6:			// Joystick 1
                    switch ( address ) {
                        case 3 : if ( m_Joystick [0].isPressed  ) retVal = 0;	break;
                        case 4 : if ( m_Joystick [0].x_Axis < 0 ) retVal = 0;	break;
                        case 5 : if ( m_Joystick [0].x_Axis > 0 ) retVal = 0;	break;
                        case 6 : if ( m_Joystick [0].y_Axis < 0 ) retVal = 0;	break;
                        case 7 : if ( m_Joystick [0].y_Axis > 0 ) retVal = 0;	break;
                    }
                    break;
                case 7:			// Joystick 2
                    switch ( address ) {
                        case 3 : if ( m_Joystick [1].isPressed  ) retVal = 0;	break;
                        case 4 : if ( m_Joystick [1].x_Axis < 0 ) retVal = 0;	break;
                        case 5 : if ( m_Joystick [1].x_Axis > 0 ) retVal = 0;	break;
                        case 6 : if ( m_Joystick [1].y_Axis < 0 ) retVal = 0;	break;
                        case 7 : if ( m_Joystick [1].y_Axis > 0 ) retVal = 0;	break;
                    }
                    break;
                default :
                    int index;
                    index = Keys [ address - 3 ][ m_ColumnSelect ];
                    if (( m_StateTable [ index ] ) != 0 ) retVal = 0;
                    break;
            }
        }
    }

    return retVal;
}

bool cTMS9901::LoadImage ( FILE * )
{
    FUNCTION_ENTRY ( this, "cTMS9901::LoadImage", true );

    return true;
}

bool cTMS9901::SaveImage ( FILE * )
{
    FUNCTION_ENTRY ( this, "cTMS9901::SaveImage", true );

    return true;
}

void cTMS9901::UpdateTimer ( UINT32 clockCycles )
{
    FUNCTION_ENTRY ( this, "cTMS9901::UpdateTimer", false );

    // Update the timer if we're in I/O mode
    if ( m_PinState [0][1] == 0 ) {
        if (( m_Decrementer != 0 ) || ( m_ClockRegister != 0 )) {
            int delta = ( clockCycles - m_DecrementClock ) / 64 + 1;
            if ( delta != m_LastDelta ) {
                int dif = delta - m_LastDelta;
                m_LastDelta = delta;
                if ( m_Decrementer >= dif ) {
                    m_Decrementer -= dif;
                } else {
                    m_Decrementer -= dif - m_ClockRegister + 1;
                    if ( m_TimerActive == true ) {
                        m_TimerActive = false;
                        SignalInterrupt ( 3 );
                    }
                }
            }
        }
    }

    m_LastClockCycle = clockCycles;
}

void cTMS9901::HardwareReset ()
{
    FUNCTION_ENTRY ( this, "cTMS9901::HardwareReset", true );
}

void cTMS9901::SoftwareReset ()
{
    FUNCTION_ENTRY ( this, "cTMS9901::SoftwareReset", true );
}

void cTMS9901::SignalInterrupt ( int level )
{
    FUNCTION_ENTRY ( this, "cTMS9901::SignalInterrupt", false );

    DBG_ASSERT ( level < 32 );

    if ( m_PinState [level][0] != 0 ) {
        // This level is already signalled - nothing more to do here
        if ( level != 2 ) {
            DBG_STATUS ( "Interrupt " << level << " already signalled" );
        }
        return;
    }

    if ( level != 2 ) {
        DBG_STATUS ( "Interrupt " << level << " signalled" );
    }

    m_InterruptRequested++;
    m_PinState [level][0] = -1;

    // If this INT line is enabled, signal an interrupt to the CPU
    if ( m_PinState [level][1] == 1 ) {
        m_ActiveInterrupts++;
        m_pCPU->SignalInterrupt ( 1 );
    }
}

void cTMS9901::ClearInterrupt ( int level )
{
    FUNCTION_ENTRY ( this, "cTMS9901::ClearInterrupt", false );

    DBG_ASSERT ( level < 32 );

    if ( level != 2 ) {
        DBG_STATUS ( "Interrupt " << level << " cleared" );
    }

    if ( m_PinState [level][0] == 0 ) return;

    m_PinState [level][0] = 0;
    m_InterruptRequested--;

    if ( m_PinState [level][1] == 1 ) {
        m_ActiveInterrupts--;
        if ( m_ActiveInterrupts == 0 ) {
            m_pCPU->ClearInterrupt ( 1 );
        }
    }
}

void cTMS9901::VKeyDown ( int sym, VIRTUAL_KEY_E vkey )
{
    FUNCTION_ENTRY ( this, "cTMS9901::VKeyDown", false );

    DBG_ASSERT (( sym >= 0 ) && ( sym < 512 ));
    DBG_ASSERT (( vkey >= VK_NONE ) && ( vkey < VK_MAX ));

    for ( unsigned i = 0; i < SIZE ( m_KSLinkTable [0]); i++ ) {
        if ( m_KSLinkTable [sym][i] == vkey ) {
            DBG_WARNING ( "Virtual key " << vkey << " already bound to " << sym );
            return;
        }
        if ( m_KSLinkTable [sym][i] == VK_NONE ) {
            m_KSLinkTable [sym][i] = vkey;
            m_StateTable [vkey]++;
            return;
        }
    }

    DBG_ERROR ( "More than 4 virtual keys on keysym " << vkey );
}

void cTMS9901::VKeysDown ( int sym, VIRTUAL_KEY_E vkey1, VIRTUAL_KEY_E vkey2 )
{
    FUNCTION_ENTRY ( this, "cTMS9901::VKeysDown", false );

    DBG_ASSERT (( sym >= 0 ) && ( sym < 512 ));
    DBG_ASSERT (( vkey1 >= VK_NONE ) && ( vkey1 < VK_MAX ));
    DBG_ASSERT (( vkey2 >= VK_NONE ) && ( vkey2 < VK_MAX ));

    if ( vkey1 != VK_NONE ) VKeyDown ( sym, vkey1 );
    if ( vkey2 != VK_NONE ) VKeyDown ( sym, vkey2 );
}

void cTMS9901::VKeyUp ( int sym )
{
    FUNCTION_ENTRY ( this, "cTMS9901::VKeyUp", false );

    DBG_ASSERT (( sym >= 0 ) && ( sym < 512 ));

    for ( unsigned i = 0; i < SIZE ( m_KSLinkTable [0]); i++ ) {
        int vkey = m_KSLinkTable[sym][i];
        if ( vkey == VK_NONE ) return;
        m_KSLinkTable [sym][i] = VK_NONE;
        DBG_ASSERT ( m_StateTable [vkey] != 0 );
        m_StateTable [vkey]--;
    }
}

void cTMS9901::SetJoystickX ( int index, int value )
{
    FUNCTION_ENTRY ( this, "cTMS9901::SetJoystickX", true );

    DBG_ASSERT (( index >= 0 ) && ( index < 2 ));

    m_Joystick [index].x_Axis = value;
}

void cTMS9901::SetJoystickY ( int index, int value )
{
    FUNCTION_ENTRY ( this, "cTMS9901::SetJoystickY", true );

    DBG_ASSERT (( index >= 0 ) && ( index < 2 ));

    m_Joystick [index].y_Axis = value;
}

void cTMS9901::SetJoystickButton ( int index, bool value )
{
    FUNCTION_ENTRY ( this, "cTMS9901::SetJoystickButton", true );

    DBG_ASSERT (( index >= 0 ) && ( index < 2 ));

    m_Joystick [index].isPressed = value;
}
