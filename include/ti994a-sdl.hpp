//----------------------------------------------------------------------------
//
// File:        ti994a-sdl.hpp
// Date:        18-Apr-2000
// Programmer:  Marc Rousseau
//
// Description:
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

#ifndef TI994A_SDL_HPP_
#define TI994A_SDL_HPP_

#include "ti994a.hpp"

struct SDL_Thread;

class cCartridge;
class cTMS9918A;
class cTMS9919;
class cTMS5220;

class cSdlTI994A : public cTI994A {

    enum WRITE_PROTECT_E {
        WRITE_PROTECT_UNKNOWN,
        WRITE_PROTECT_BANK1,
        WRITE_PROTECT_ENABLED,
        WRITE_PROTECT_BANK2,
        WRITE_PROTECT_MAX
    };

    UINT32              m_StartTime;
    UINT32              m_StartClock;

    SDL_Thread         *m_pThread;
    SDL_sem            *m_SleepSem;
    SDL_sem            *m_WaitSem;

    int                 m_JoystickMap [2];
    int                 m_JoystickPosX [2];
    int                 m_JoystickPosY [2];

    cCartridge         *m_pGramKracker;

    WRITE_PROTECT_E     m_GK_WriteProtect;

    bool                m_GK_Enabled;
    bool                m_GK_OpSys;
    bool                m_GK_BASIC;
    bool                m_GK_LoaderOn;

public:

    cSdlTI994A ( cCartridge *, cTMS9918A * = NULL, cTMS9919 * = NULL, cTMS5220 * = NULL );
    ~cSdlTI994A ();

    //
    // cTI994A virtual functions
    //
    virtual void Sleep ( int, UINT32 );
    virtual void WakeCPU ( UINT32 );

    virtual void InsertCartridge ( cCartridge *, bool = true );
    virtual void RemoveCartridge ( cCartridge *, bool = true );

    virtual void Reset ();

    virtual void Run ();

    virtual bool SaveImage ( const char * );
    virtual bool LoadImage ( const char * );

    void SetJoystick ( int, SDL_Joystick * );

protected:

    int FindJoystick ( int );

    void KeyPressed ( SDL_keysym keysym );
    void KeyReleased ( SDL_keysym keysym );

    void StartThread ();
    void StopThread ();

    void SetWriteProtect ( WRITE_PROTECT_E );

    void GK_ToggleEnabled ();
    void GK_ToggleOpSys ();
    void GK_ToggleBASIC ();
    void GK_ToggleLoader ();

    virtual int TimerHookProc ();

    static int _RunThreadProc ( void * );
    int RunThreadProc ();

private:

    cSdlTI994A ( const cSdlTI994A & );      // no implementation
    void operator = ( const cSdlTI994A & ); // no implementation

};

#endif
