//----------------------------------------------------------------------------
//
// File:        tms9918a.hpp
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

#ifndef TMS9918A_HPP_
#define TMS9918A_HPP_

#include "tms9900.hpp"

class cTMS9901;

#define TI_TRANSPARENT          0x00
#define TI_BLACK                0x01
#define TI_MEDIUM_GREEN         0x02
#define TI_LIGHT_GREEN          0x03
#define TI_DARK_BLUE            0x04
#define TI_LIGHT_BLUE           0x05
#define TI_DARK_RED             0x06
#define TI_CYAN                 0x07
#define TI_MEDIUM_RED           0x08
#define TI_LIGHT_RED            0x09
#define TI_DARK_YELLOW          0x0A
#define TI_LIGHT_YELLOW         0x0B
#define TI_DARK_GREEN           0x0C
#define TI_MAGENTA              0x0D
#define TI_GRAY                 0x0E
#define TI_WHITE                0x0F

#define VDP_TIMER               0L
#define VPD_INTERRUPT_INTERVAL  17              // 60 Hz (16.666 msec)

#define VDP_WIDTH               256
#define VDP_HEIGHT              192

#define MEM_IMAGE_TABLE         0x01
#define MEM_PATTERN_TABLE       0x02
#define MEM_COLOR_TABLE         0x04
#define MEM_SPRITE_ATTR_TABLE   0x10
#define MEM_SPRITE_DESC_TABLE   0x20

// VDP Register 0
#define VDP_MODE_3_BIT          0x02
#define VDP_EXTERNAL_VIDEO_MASK 0x01

// VDP Register 1
#define VDP_16K_MASK            0x80
#define VDP_BLANK_MASK          0x40
#define VDP_INTERRUPT_MASK      0x20
#define VDP_MODE_1_BIT          0x10
#define VDP_MODE_2_BIT          0x08
#define VDP_SPRITE_SIZE         0x02
#define VDP_SPRITE_MAGNIFY      0x01
#define VDP_SPRITE_MASK         0x03


#define VDP_M3                  0x01    // BitMapped
#define VDP_M2                  0x02    // Multi-Color
#define VDP_M1                  0x04    // Text

#define VDP_INTERRUPT_FLAG      0x80
#define VDP_FIFTH_SPRITE_FLAG   0x40
#define VDP_COINCIDENCE_FLAG    0x20
#define VDP_FIFTH_SPRITE_MASK   0x1F

struct sScreenImage {
    union {
        UINT8                grapPos [ 24 ][ 32 ];
        UINT8                textPos [ 24 ][ 40 ];
        UINT8                data [ 0x0400 ];
    };
};

struct sColorTable {
    UINT8                    data [64];
};

struct sPatternDescriptor {
    UINT8                    data [256][8];
};

struct sSpriteAttributeEntry {
    UINT8                    posY;
    UINT8                    posX;
    UINT8                    patternIndex;
    UINT8                    earlyClock;
};

struct sSpriteAttribute {
    sSpriteAttributeEntry    data [32];
};

struct sSpriteDescriptor {
    UINT8                    data [256][8];
};

class cTMS9918A {

protected:

    UINT8              *m_Memory;

    sScreenImage       *m_ImageTable;
    sColorTable        *m_ColorTable;
    sPatternDescriptor *m_PatternTable;
    sSpriteAttribute   *m_SpriteAttrTable;
    sSpriteDescriptor  *m_SpriteDescTable;

    int                 m_ImageTableSize;
    int                 m_ColorTableSize;
    int                 m_PatternTableSize;

    int                 m_InterruptLevel;
    cTMS9901           *m_PIC;

    ADDRESS             m_Address;
    UINT16              m_Transfer;
    UINT16              m_Shift;

    UINT8               m_Status;
    UINT8               m_Register [8];
    UINT8               m_Mode;

    UINT8               m_ReadAhead;

    UINT8               m_MemoryType [0x4000];

    UINT8               m_MaxSprite [256];
    bool                m_SpritesDirty;
    bool                m_SpritesRefreshed;
    bool                m_CoincidenceFlag;
    bool                m_FifthSpriteFlag;
    int                 m_FifthSpriteIndex;

    int                 m_RefreshRate;

    virtual bool SetMode ( int );
    virtual void Refresh ( bool )		{}

    virtual void FlipAddressing ();

    void FillTable ( int, int, UINT8 );

    void GetSpritePattern ( int index, int loX, int hiX, int loY, int hiY, int data [32] );
    bool SpritesCoincident ( int, int );
    bool CheckCoincidence ( const bool [32] );

    void CheckSprites ();

public:

    cTMS9918A ( int = 60 );
    virtual ~cTMS9918A ();

    void SetPIC ( cTMS9901 *, int );

    virtual void Retrace ();

    virtual void Reset ();
    virtual void WriteAddress ( UINT8 );
    virtual void WriteData ( UINT8 );
    virtual void WriteRegister ( size_t, UINT8 );

    virtual UINT8 ReadData ();
    virtual UINT8 ReadRegister ( size_t reg )	{ return m_Register [ reg ]; }
    virtual UINT8 ReadStatus ();

    virtual bool BlankEnabled ()		{ return ( m_Register [1] & VDP_BLANK_MASK ) ? false : true; }
    virtual bool InterruptsEnabled ()		{ return ( m_Register [1] & VDP_INTERRUPT_MASK ) ? true : false; }

    virtual ADDRESS GetAddress ()		{ return ( ADDRESS ) ( m_Address & 0x3FFF ); }

    virtual bool LoadImage ( FILE * );
    virtual bool SaveImage ( FILE * );

    int    GetRefreshRate ()			{ return m_RefreshRate; }

    int    GetMode () const			{ return m_Mode; }
    UINT8  *GetMemory () const			{ return m_Memory; }

    ADDRESS GetImageTable () const		{ return ( ADDRESS ) (( UINT8 * ) m_ImageTable - m_Memory ); }
    ADDRESS GetColorTable () const		{ return ( ADDRESS ) (( UINT8 * ) m_ColorTable - m_Memory ); }
    ADDRESS GetPatternTable () const		{ return ( ADDRESS ) (( UINT8 * ) m_PatternTable - m_Memory ); }
    ADDRESS GetSpriteAttrTable () const		{ return ( ADDRESS ) (( UINT8 * ) m_SpriteAttrTable - m_Memory ); }
    ADDRESS GetSpriteDescTable () const		{ return ( ADDRESS ) (( UINT8 * ) m_SpriteDescTable - m_Memory ); }

private:

    cTMS9918A ( const cTMS9918A & );        // no implementation
    void operator = ( const cTMS9918A & );  // no implementation

};

#endif
