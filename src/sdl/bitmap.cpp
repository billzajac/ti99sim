//----------------------------------------------------------------------------
//
// File:        bitmap.cpp
// Date:        06-Apr-2000
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

#include <stdio.h>
#include "common.hpp"
#include "logger.hpp"
#include "tms9900.hpp"
#include "tms9918a.hpp"
#include "ti994a.hpp"
#include "SDL.h"
#include "bitmap.hpp"
#include "tms9918a-sdl.hpp"

DBG_REGISTER ( __FILE__ );

cBitMap::cBitMap ( SDL_Surface *surface, bool useScale2x ) :
    m_Scale2x ( useScale2x ),
    m_Width ( 0 ),
    m_Height ( 0 ),
    m_Pitch ( 0 ),
    m_pSurface ( surface )
{
    FUNCTION_ENTRY ( this, "cBitMap ctor", true );

    if ( m_pSurface != NULL ) {
      m_Width  = m_pSurface->w;
      m_Height = m_pSurface->h;
      m_Pitch  = m_pSurface->pitch;
    }
}

cBitMap::~cBitMap ()
{
    FUNCTION_ENTRY ( this, "cBitMap dtor", true );

    SDL_FreeSurface ( m_pSurface );
}

void cBitMap::SetPalette ( SDL_Color *palette, int count )
{
    FUNCTION_ENTRY ( this, "cBitMap::SetPalette", true );

    SDL_SetColors ( m_pSurface, palette, 0, count );
}

void cBitMap::LockSurface ()
{
    FUNCTION_ENTRY ( this, "cBitMap::LockSurface", false );

    if ( SDL_MUSTLOCK ( m_pSurface )) {
        SDL_LockSurface ( m_pSurface );
    }
}

void cBitMap::UnlockSurface ()
{
    FUNCTION_ENTRY ( this, "cBitMap::UnlockSurface", false );

    if ( SDL_MUSTLOCK ( m_pSurface )) {
        SDL_UnlockSurface ( m_pSurface );
    }
}

template<class T> inline void cBitMap::Scale ( cBitMap *original, int scale, UINT8 *pDstData )
{
    FUNCTION_ENTRY ( this, "Scale<>", false );

    UINT8 *pSrcData = original->GetData ();

    int width  = original->Width ();
    int height = original->Height ();

    for ( int y = 0; y < height; y++ ) {
        T *pData = ( T * ) pDstData;
        for ( int x = 0; x < width; x++ ) {
            for ( int i = 0; i < scale; i++ ) {
                *pData++ = (( T * ) pSrcData) [x];
            }
        }
        for ( int i = 1; i < scale; i++ ) {
            memcpy ( pDstData + Pitch (), pDstData, Width () * sizeof ( T ));
            pDstData += Pitch ();
        }
        pSrcData += original->Pitch ();
        pDstData += Pitch ();
    }
}

template<class T> void CalculatePixels ( int B, int D, int E, int F, int H, T &E0, T &E1, T &E2, T &E3 )
{
    // Here are the equations from the scale2x website (http://scale2x.sourceforge.net/algorithm.html):
    //
    //   E0 = D == B && B != F && D != H ? D : E;
    //   E1 = B == F && B != D && F != H ? F : E;
    //   E2 = D == H && D != B && H != F ? D : E;
    //   E3 = H == F && D != H && B != F ? F : E;
    //
    // Re-arranging terms and inverting the logic for E1 and E2 we get:
    //
    //   E0 = B == D && B != F && D != H           ? D : E;
    //   E1 = B == D || B != F ||           H == F ? E : F;
    //   E2 = B == D ||           D != H || H == F ? E : D;
    //   E3 =           B != F && D != H && H == F ? F : E;
    //
    // The following code eliminates redundant comparisions:

    if ( B == D ) {
        E1 = ( T ) E;
        E2 = ( T ) E;
        if ( B != F ) {
            if ( D != H ) {
                E0 = ( T ) D;
                E3 = ( T ) (( H == F ) ? F : E );
            } else {
                E0 = ( T ) E;
                E3 = ( T ) E;
            }
        } else {
            E0 = ( T ) E;
            E3 = ( T ) E;
        }
    } else {
        E0 = ( T ) E;
        if ( B != F ) {
            E1 = ( T ) E;
            if ( D != H ) {
                E2 = ( T ) E;
                E3 = ( T ) (( H == F ) ? F : E );
            } else {
                E2 = ( T ) (( H == F ) ? E : D );
                E3 = ( T ) E;
            }
        } else {
            E3 = ( T ) E;
            if ( H == F ) {
                E1 = ( T ) E;
                E2 = ( T ) E;
            } else {
                E1 = ( T ) F;
                E2 = ( T ) (( D != H ) ? E : D );
            }
        }
    }
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT8 *pData1, UINT8 *pData2 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT8> ( B, D, E, F, H, pData1 [0], pData1 [1], pData2 [0], pData2 [1] );
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT16 *pData1, UINT16 *pData2 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT16> ( B, D, E, F, H, pData1 [0], pData1 [1], pData2 [0], pData2 [1] );
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT32 *pData1, UINT32 *pData2 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT32> ( B, D, E, F, H, pData1 [0], pData1 [1], pData2 [0], pData2 [1] );
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT8 *pData1, UINT8 *pData2, UINT8 *pData3 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT8> ( B, D, E, F, H, pData1 [0], pData1 [2], pData3 [0], pData3 [2] );

    pData1 [1] = ( UINT8 ) E;
    pData2 [0] = ( UINT8 ) E;
    pData2 [1] = ( UINT8 ) E;
    pData2 [2] = ( UINT8 ) E;
    pData3 [1] = ( UINT8 ) E;
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT16 *pData1, UINT16 *pData2, UINT16 *pData3 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT16> ( B, D, E, F, H, pData1 [0], pData1 [2], pData3 [0], pData3 [2] );

    pData1 [1] = ( UINT16 ) E;
    pData2 [0] = ( UINT16 ) E;
    pData2 [1] = ( UINT16 ) E;
    pData2 [2] = ( UINT16 ) E;
    pData3 [1] = ( UINT16 ) E;
}

void cBitMap::CalculateNewPixels ( int B, int D, int E, int F, int H, UINT32 *pData1, UINT32 *pData2, UINT32 *pData3 )
{
    FUNCTION_ENTRY ( this, "cBitMap::CalculateNewPixels", false );

    CalculatePixels<UINT32> ( B, D, E, F, H, pData1 [0], pData1 [2], pData3 [0], pData3 [2] );

    pData1 [1] = ( UINT32 ) E;
    pData2 [0] = ( UINT32 ) E;
    pData2 [1] = ( UINT32 ) E;
    pData2 [2] = ( UINT32 ) E;
    pData3 [1] = ( UINT32 ) E;
}

template<class T> void cBitMap::Scale2xImp ( cBitMap *original, UINT8 *pDstData )
{
    FUNCTION_ENTRY ( this, "cBitMap::Scale2xImp<>", false );

    UINT8 *pSrcData = original->GetData ();

    int width  = original->Width ();
    int height = original->Height ();

    int B, D, E, F, H;

    T background = * ( T * ) pDstData;

    T *pLstData = ( T * ) ( pSrcData );
    T *pCurData = ( T * ) ( pSrcData );
    T *pNxtData = ( T * ) ( pSrcData + original->Pitch ());

    {
        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());

        B = background;
        E = background;
        F = pCurData [0];

        for ( int x = 0; x < width - 1; x++ ) {

            D = E;
            E = F;
            F = pCurData [ x + 1 ];
            H = pNxtData [ x ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2 );

            pData1 += 2;
            pData2 += 2;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2 );

        pLstData  = pCurData;
        pCurData  = pNxtData;
        pNxtData += original->Pitch () / sizeof ( T );
        pDstData += 2 * Pitch ();
    }

    for ( int y = 1; y < height - 1; y++ ) {

        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());

        E = background;
        F = pCurData [0];

        for ( int x = 0; x < width - 1; x++ ) {

            B = pLstData [ x ];
            D = E;
            E = F;
            F = pCurData [ x + 1 ];
            H = pNxtData [ x ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2 );

            pData1 += 2;
            pData2 += 2;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2 );

        pLstData  = pCurData;
        pCurData  = pNxtData;
        pNxtData += original->Pitch () / sizeof ( T );
        pDstData += 2 * Pitch ();
    }

    {
        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());

        E = background;
        F = pCurData [0];
        H = background;

        for ( int x = 0; x < width - 1; x++ ) {

            B = pLstData [ x ];
            D = E;
            E = F;
            F = pCurData [ x + 1 ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2 );

            pData1 += 2;
            pData2 += 2;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2 );
    }
}

template<class T> void cBitMap::Scale3xImp ( cBitMap *original, UINT8 *pDstData )
{
    FUNCTION_ENTRY ( this, "cBitMap::Scale3xImp<>", false );

    UINT8 *pSrcData = original->GetData ();

    int width  = original->Width ();
    int height = original->Height ();

    int B, D, E, F, H;

    T background = * ( T * ) pDstData;

    T *pLstData = ( T * ) ( pSrcData );
    T *pCurData = ( T * ) ( pSrcData );
    T *pNxtData = ( T * ) ( pSrcData + original->Pitch ());

    {
        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());
        T *pData3 = ( T * ) ( pDstData + Pitch () * 2 );

        B = background;
        E = background;
        F = pCurData [0];

        for ( int x = 0; x < width - 1; x++ ) {

            D = E;
            E = F;
            F = pCurData [ x + 1 ];
            H = pNxtData [ x ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2, pData3 );

            pData1 += 3;
            pData2 += 3;
            pData3 += 3;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2, pData3 );

        pLstData  = pCurData;
        pCurData  = pNxtData;
        pNxtData += original->Pitch () / sizeof ( T );
        pDstData += 3 * Pitch ();
    }

    for ( int y = 1; y < height - 1; y++ ) {

        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());
        T *pData3 = ( T * ) ( pDstData + Pitch () * 2 );

        E = background;
        F = pCurData [0];

        for ( int x = 0; x < width - 1; x++ ) {

            B = pLstData [ x ];
            D = E;
            E = F;
            F = pCurData [ x + 1 ];
            H = pNxtData [ x ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2, pData3 );

            pData1 += 3;
            pData2 += 3;
            pData3 += 3;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2, pData3 );

        pLstData  = pCurData;
        pCurData  = pNxtData;
        pNxtData += original->Pitch () / sizeof ( T );
        pDstData += 3 * Pitch ();
    }

    {
        T *pData1 = ( T * ) ( pDstData );
        T *pData2 = ( T * ) ( pDstData + Pitch ());
        T *pData3 = ( T * ) ( pDstData + Pitch () * 2 );

        E = background;
        F = pCurData [0];
        H = background;

        for ( int x = 0; x < width - 1; x++ ) {

            B = pLstData [ x ];
            D = E;
            E = F;
            F = pCurData [ x + 1 ];

            CalculateNewPixels ( B, D, E, F, H, pData1, pData2, pData3 );

            pData1 += 3;
            pData2 += 3;
            pData3 += 3;
        }

        CalculateNewPixels ( B, E, F, background, pNxtData [ width - 1 ], pData1, pData2, pData3 );
    }
}

void cBitMap::Scale2X ( cBitMap *original )
{
    FUNCTION_ENTRY ( this, "cBitMap::Scale2X", false );

    int wDif = Width () - original->Width () * 2;
    int hDif = Height () - original->Height () * 2;

    LockSurface ();
    original->LockSurface ();

    UINT8 *pDstData = GetData ();

    if ( wDif > 0 ) {
        pDstData += wDif / 2 * m_pSurface->format->BytesPerPixel;
    }

    if ( hDif > 0 ) {
        pDstData += hDif / 2 * Pitch ();
    }

    switch ( m_pSurface->format->BytesPerPixel ) {
        case 1 :
            Scale2xImp<UINT8> ( original, pDstData );
            break;
        case 2 :
            Scale2xImp<UINT16> ( original, pDstData );
            break;
        case 4 :
            Scale2xImp<UINT32> ( original, pDstData );
            break;
    }

    original->UnlockSurface ();
    UnlockSurface ();
}

void cBitMap::Scale3X ( cBitMap *original )
{
    FUNCTION_ENTRY ( this, "cBitMap::Scale3X", false );

    int wDif = Width () - original->Width () * 3;
    int hDif = Height () - original->Height () * 3;

    LockSurface ();
    original->LockSurface ();

    UINT8 *pDstData = GetData ();

    if ( wDif > 0 ) {
        pDstData += wDif / 2 * m_pSurface->format->BytesPerPixel;
    }

    if ( hDif > 0 ) {
        pDstData += hDif / 2 * Pitch ();
    }

    switch ( m_pSurface->format->BytesPerPixel ) {
        case 1 :
            Scale3xImp<UINT8> ( original, pDstData );
            break;
        case 2 :
            Scale3xImp<UINT16> ( original, pDstData );
            break;
        case 4 :
            Scale3xImp<UINT32> ( original, pDstData );
            break;
    }

    original->UnlockSurface ();
    UnlockSurface ();
}

void cBitMap::Copy ( cBitMap *original )
{
    FUNCTION_ENTRY ( this, "cBitMap::Copy", false );

    int scaleX = Width () / original->Width ();
    int scaleY = Height () / original->Height ();

    int scale = max ( min ( scaleX, scaleY ), 1 );

    int wDif = Width () - original->Width () * scale;
    int hDif = Height () - original->Height () * scale;

    // See if we can use the SDL blit
    if ( scale == 1 ) {

        SDL_Rect srcRect = { 0, 0, original->Width (), original->Height () };
        SDL_Rect dstRect = { 0, 0, m_Width, m_Height };

        // Center or crop as necessary
        if ( wDif != 0 ) {
            if ( wDif > 0 ) {
                dstRect.x = wDif / 2;
            } else {
                srcRect.x = -wDif / 2;
                srcRect.w = Width ();
            }
        }

        if ( hDif != 0 ) {
            if ( hDif > 0 ) {
                dstRect.y = hDif / 2;
            } else {
                srcRect.y = -hDif / 2;
                srcRect.h = Height ();
            }
        }

        SDL_BlitSurface ( original->m_pSurface, &srcRect, m_pSurface, &dstRect );

    } else if (( m_Scale2x == true ) && ( scale == 2 )) {

        Scale2X ( original );

    } else if (( m_Scale2x == true ) && ( scale == 3 )) {

        Scale3X ( original );

    } else {

        LockSurface ();
        original->LockSurface ();

        UINT8 *pDstData = GetData ();

        if ( wDif > 0 ) {
            pDstData += wDif / 2 * m_pSurface->format->BytesPerPixel;
        }

        if ( hDif > 0 ) {
            pDstData += hDif / 2 * Pitch ();
        }

        switch ( m_pSurface->format->BytesPerPixel ) {
            case 1 :
                Scale<UINT8> ( original, scale, pDstData );
                break;
            case 2 :
                Scale<UINT16> ( original, scale, pDstData );
                break;
            case 4 :
                Scale<UINT32> ( original, scale, pDstData );
                break;
        }

        original->UnlockSurface ();
        UnlockSurface ();

    }
}
