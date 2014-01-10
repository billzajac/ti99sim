//----------------------------------------------------------------------------
//
// File:        bitmap.hpp
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

#ifndef BITMAP_HPP_
#define BITMAP_HPP_

struct sRECT {
    int Left;
    int Right;
    int Top;
    int Bottom;
};

enum COLOR_INDEX_E {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE
};

struct sBitMapInfo {
    int          BitsPerPixel;
    int          BytesPerPixel;
    int          ColorMask [3];
    int          ColorShift [3];
};

class cBitMap {

private:

    bool         m_Scale2x;
    int          m_Width;
    int          m_Height;
    int          m_Pitch;
    SDL_Surface *m_pSurface;

    template<class T> void Scale ( cBitMap *, int, UINT8 * );

    // Routines for Scale2x
    void CalculateNewPixels ( int, int, int, int, int, UINT8 *, UINT8 * );
    void CalculateNewPixels ( int, int, int, int, int, UINT16 *, UINT16 * );
    void CalculateNewPixels ( int, int, int, int, int, UINT32 *, UINT32 * );

    // Routines for Scale2x
    void CalculateNewPixels ( int, int, int, int, int, UINT8 *, UINT8 *, UINT8 * );
    void CalculateNewPixels ( int, int, int, int, int, UINT16 *, UINT16 *, UINT16 * );
    void CalculateNewPixels ( int, int, int, int, int, UINT32 *, UINT32 *, UINT32 * );

    template<class T> void Scale2xImp ( cBitMap *, UINT8 * );
    template<class T> void Scale3xImp ( cBitMap *, UINT8 * );

    void Scale2X ( cBitMap * );
    void Scale3X ( cBitMap * );

public:

    cBitMap ( SDL_Surface *, bool );
    virtual ~cBitMap ();

    int    Width () const		{ return m_Width;  }
    int    Height () const		{ return m_Height; }
    int    Pitch () const		{ return m_Pitch; }

    void   SetPalette ( SDL_Color *, int );
    void   LockSurface ();
    void   UnlockSurface ();
    UINT8 *GetData () const		{ return ( m_pSurface != NULL ) ? ( UINT8 * ) m_pSurface->pixels : NULL; }
    SDL_Surface *GetSurface () const    { return m_pSurface; }

    void  Copy ( cBitMap * );

private:

    cBitMap ( const cBitMap & );         // no implementation
    void operator = ( const cBitMap & ); // no implementation

};

#endif
