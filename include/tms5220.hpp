//----------------------------------------------------------------------------
//
// File:        tms5220.hpp
// Date:        27-Nov-2000
// Programmer:  Marc Rousseau
//
// Description: Default class for the TMS5220 Speech Synthesizer Chip
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

#ifndef TMS5220_HPP_
#define TMS5220_HPP_

#define TMS5220_TS  0x80    // Talk Status
#define TMS5220_BL  0x40    // Buffer Low
#define TMS5220_BE  0x20    // Buffer Empty

#define SPEECH_COMMAND_MASK 0x70

#define REFLECTION_K1       0
#define REFLECTION_K2       1
#define REFLECTION_K3       2
#define REFLECTION_K4       3
#define REFLECTION_K5       4
#define REFLECTION_K6       5
#define REFLECTION_K7       6
#define REFLECTION_K8       7
#define REFLECTION_K9       8
#define REFLECTION_K10      9

#define FIFO_BYTES         16
#define FIFO_BITS          (FIFO_BYTES * 8)

const int TMS5220_FRAME_RATE             = 40;      // 40 Hz
const int TMS5220_FRAME_PERIOD           = 25000;   // 25 ms
const int TMS5220_INTERPOLATION_RATE     = 320;     // 320 Hz
const int TMS5220_INTERPOLATION_INTERVAL = 3125;    // 3.125 ms
const int TMS5220_SAMPLE_RATE            = 8000;    // 8 KHz
const int TMS5220_SAMPLE_PERIOD          = 125;     // 125 us
const int TMS5220_ROM_CLOCK_RATE         = 160000;  // 160 KHz
const int TMS5220_ROM_CLOCK_PERIOD       = 6;       // 6.25 us
const int TMS5220_RC_OSC_RATE            = 640000;  // 640 KHz
const int TMS5220_RC_OSC_PERIOD          = 1;       // 1562.5 ns

const int RC_ORDER               = 10;      // # of Reflection Coefficients
const int SAMPLE_RATE            = 8000;    // 8 KHz
const int INTERPOLATION_INTERVAL = 25;      // 3.125 ms

class cTI994A;
class cTMS9919;

class cTMS5220 {

protected:

    struct sSpeechParams {
        int        Energy;
        int        Pitch;
        bool       Repeat;
        bool       Stop;
        double     Reflection [ RC_ORDER ];
        double     Gain;
    };

    union sReadState {
        struct {
            UINT16 Address;
            UINT16 ChipSelect;
            int    BitsUsed;
        } rom;
        struct {
            int    GetIndex;
            int    PutIndex;
            int    BitsLeft;
        } fifo;
    };

    UINT8         *m_SpeechRom;

    // ROM address information
    int            m_LoadPointer;
    UINT16         m_Address;
    UINT16         m_ChipSelect;
    UINT8          m_VsmData;
    int            m_VsmBitsLeft;

    // 16-byte/128-bit parallel-serial FIFO
    UINT8          m_FIFO [ FIFO_BYTES ];
    volatile int   m_GetIndex;
    volatile int   m_PutIndex;
    volatile int   m_BitsLeft;

    // State variables
    bool           m_ReadByte;
    bool           m_SpeakExternal;

    // Hardware registers
    bool           m_BufferEmpty;
    bool           m_TalkStatus;
    UINT8          m_Data;
    UINT8          m_Command;

    // Current/Target parameters & interpolation information
    sSpeechParams  m_StartParams;
    sSpeechParams  m_TargetParams;
    int            m_InterpolationStage;

    cTI994A       *m_Computer;
    cTMS9919      *m_SoundChip;

    // 8 KHz speech buffer
    int            m_PitchIndex;
    double         m_NonVoicedLevel;
    double         m_FilterHistory [ 2 ][ RC_ORDER + 1 ];
    double         m_RawDataBuffer [ INTERPOLATION_INTERVAL ];

    // Resampled synthesized speech buffer
    int            m_PlaybackFrequency;
    int            m_PlaybackInterval;
    double        *m_PlaybackBuffer;
    int            m_PlaybackSamplesLeft;
    double        *m_PlaybackDataPtr;

    void LoadAddress ( UINT8 data );

    void SaveReadState ( sReadState * ) const;
    void RestoreReadState ( const sReadState & );

    UINT8 ReadBits ( int );
    UINT8 ReadBitsROM ( int );
    UINT8 ReadBitsFIFO ( int );

    void StoreDataFIFO ( UINT8 data );

    bool CreateNextBuffer ();
    bool ConvertBuffer ();
    bool GetNextBuffer ();

    static char *FormatParameters ( const sSpeechParams &, bool );
    static void InterpolateParameters ( int,  sSpeechParams &, const sSpeechParams &, sSpeechParams * );

    bool ReadFrame ( sSpeechParams *, bool );

public:

    cTMS5220 ( cTMS9919 * );
    virtual ~cTMS5220 ();

    void SetComputer ( cTI994A * );

    virtual bool AudioCallback ( UINT8 *, int );

    virtual void Reset ();

    UINT8 WriteData ( UINT8 );
    UINT8 ReadData ( UINT8 );

private:

    cTMS5220 ( const cTMS5220 & );        // no implementation
    void operator = ( const cTMS5220 & ); // no implementation

};

#endif
