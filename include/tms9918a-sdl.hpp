//----------------------------------------------------------------------------
//
// File:        tms9918a-sdl.hpp
// Date:        02-Apr-2000
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

#ifndef TMS9918A_SDL_HPP_
#define TMS9918A_SDL_HPP_

#include "tms9918a.hpp"

class cBitMap;

#define FILL_SIZE		256

typedef SDL_Color  sRGBQUAD;
typedef SDL_mutex *MUTEX;

class cSdlTMS9918A : public cTMS9918A {

    sRGBQUAD      m_RawColorTable [17];
    sRGBQUAD      m_SDLColorTable [17];

    int           m_TableSize;
    bool          m_TextMode;
    bool          m_ChangesMade;
    bool          m_BlankChanged;
    bool          m_ColorsChanged;
    bool          m_SpritesChanged;
    bool          m_NeedsUpdate;

    bool          m_ScreenChanged [ 0x03C0 ];

    bool          m_PatternChanged [ 256 * 3 ];
    int           m_CharUse        [ 256 * 3 ];
    int           m_SpriteCharUse  [ 256 ];

    bool          m_Scale2x;
    cBitMap      *m_Screen;
    cBitMap      *m_BitmapScreen;
    cBitMap      *m_BitmapSpriteScreen;
    UINT8        *m_CharacterPattern;
    int           m_BytesPerPixel;

    SDL_mutex    *m_Mutex;

    bool          m_FullScreen;

    int           m_OnFrames;
    int           m_OffFrames;
    int           m_FrameCycle;

    cBitMap *CreateMainWindow ( int, int );
    cBitMap *CreateMainWindowFullScreen ( int, int );
    cBitMap *CreateBitMap ( int, int );

    void ConvertColors ();

    void DrawSprite ( int );
    cBitMap *UpdateSprites ();

    void UpdateCharacterPatternGraphics ( int, UINT8, UINT8, UINT8 * );
    void UpdateCharacterPatternBitMap ( int, UINT8 * );

    void UpdateScreenGraphics ( int x, int y, int ch );
    void UpdateScreenText ( int x, int y, int ch );
    void UpdateScreenMultiColor ( int x, int y, int ch );

    int ScreenSize ()		{ return m_TextMode ? 24 * 40 : 24 * 32; }
    int GetCellWidth ()		{ return m_TextMode ? 6 : 8; }
    int GetScreenWidth ()	{ return m_TextMode ? 40 : 32; }
    int GetScreenHeight ()	{ return 24; }

    void MarkScreenChanges ( int );

    bool RefreshGraphics ();
    bool RefreshBitMap ();
    bool RefreshMultiColor ();

    void BlankScreen ( bool );
    void UpdateScreen ();

    // cTMS9918A protected methods
    virtual bool SetMode ( int );
    virtual void Refresh ( bool );
    virtual void FlipAddressing ();

public:

    cSdlTMS9918A ( sRGBQUAD [17], int = 60, bool = false, bool = false, int = 0, int = 0 );
    ~cSdlTMS9918A ();

    static int GetFullScreenResolutions ( int *x, int *y, int n );

    void SetColorTable ( sRGBQUAD [17] );
    void SetFrameRate ( int, int );

    void ResizeWindow ( int x, int y );

    cBitMap *GetScreen ();

    // cTMS9918A public methods
    virtual void Reset ();
    virtual void WriteData ( UINT8 );
    virtual void WriteRegister ( size_t, UINT8 );
    virtual bool LoadImage ( FILE * );

private:

    cSdlTMS9918A ( const cSdlTMS9918A & );    // no implementation
    void operator = ( const cSdlTMS9918A & ); // no implementation

};

#endif
