//----------------------------------------------------------------------------
//
// File:        tms9901.hpp
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

#ifndef TMS9901_HPP_
#define TMS9901_HPP_

#include "device.hpp"

//
// Virtual keys
//

enum VIRTUAL_KEY_E {
    VK_NONE,
    VK_ENTER, VK_SPACE, VK_COMMA, VK_PERIOD, VK_DIVIDE,
    VK_SEMICOLON, VK_EQUALS, VK_CAPSLOCK,
    VK_SHIFT, VK_CTRL, VK_FCTN,
    VK_0, VK_1, VK_2, VK_3, VK_4,
    VK_5, VK_6, VK_7, VK_8, VK_9,
    VK_A, VK_B, VK_C, VK_D, VK_E, VK_F, VK_G,
    VK_H, VK_I, VK_J, VK_K, VK_L, VK_M, VK_N,
    VK_O, VK_P, VK_Q, VK_R, VK_S, VK_T, VK_U,
    VK_V, VK_W, VK_X, VK_Y, VK_Z,
    VK_MAX
};

class cTMS9901 : public cDevice {

    struct sJoystickInfo {
        bool     isPressed;
        int      x_Axis;
        int      y_Axis;
    };

    bool                m_TimerActive;
    int                 m_ReadRegister;
    int                 m_Decrementer;
    int                 m_ClockRegister;
    char                m_PinState [32][2];
    int                 m_InterruptRequested;
    int                 m_ActiveInterrupts;

    int                 m_LastDelta;
    UINT32              m_DecrementClock;
    UINT32              m_LastClockCycle;

    bool                m_CapsLock;
    int                 m_ColumnSelect;
    UINT8               m_StateTable [VK_MAX];
    VIRTUAL_KEY_E       m_KSLinkTable [512][2];
    sJoystickInfo       m_Joystick [2];

public:

    cTMS9901 ( cTMS9900 * );
    virtual ~cTMS9901 ();

    //
    // cDevice methods
    //
    virtual void WriteCRU ( ADDRESS, int );
    virtual int  ReadCRU ( ADDRESS );
    virtual bool LoadImage ( FILE * );
    virtual bool SaveImage ( FILE * );

    void UpdateTimer ( UINT32 );
    void HardwareReset ();
    void SoftwareReset ();

    //
    // Methods used by devices to signal an interrupt to the CPU
    //
    void SignalInterrupt ( int );
    void ClearInterrupt ( int );

    //
    // Methods to handle the keyboard/joysticks
    //
    void VKeyUp ( int sym );
    void VKeyDown ( int sym, VIRTUAL_KEY_E vkey );
    void VKeysDown (int sym, VIRTUAL_KEY_E, VIRTUAL_KEY_E = VK_NONE );

    UINT8 GetKeyState ( VIRTUAL_KEY_E );

    void SetJoystickX ( int, int );
    void SetJoystickY ( int, int );
    void SetJoystickButton ( int, bool );

};

inline UINT8 cTMS9901::GetKeyState ( VIRTUAL_KEY_E vkey )       { return m_StateTable [vkey]; }

#endif
