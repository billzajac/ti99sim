//----------------------------------------------------------------------------
//
// File:        screenio.cpp
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

#if defined ( OS_LINUX ) || defined ( OS_MACOSX )
    #include <signal.h>
    #include <stdarg.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/time.h>
    #include <termios.h>
    #include <unistd.h>
#elif defined ( OS_WINDOWS )
    #include <conio.h>
    #include <signal.h>
    #include <windows.h>
#elif defined ( OS_OS2 )
    #define INCL_SUB
    #define INCL_NOPMAPI
    #include <os2.h>
    #include <conio.h>
#else
    #error "This code must be compiled for either OS/2, Windows, Linux, or Mac"
#endif

#include "common.hpp"
#include "screenio.hpp"

static size_t curX = 0;
static size_t curY = 0;

#define toHex(x)	( char ) (((x) > 9 ) ? (x) + 'A' - 10 : (x) + '0' )

#if defined ( OS_OS2 )

static HVIO hVio = 0;
static VIOCURSORINFO vioco;
static int oldAttr;

void SaveConsoleSettings ()
{
    VioGetCurType ( &vioco, hVio );
    oldAttr = vioco.attr;
    vioco.attr = 0xFFFF;
    VioSetCurType ( &vioco, hVio );
}

void RestoreConsoleSettings ()
{
    vioco.attr = oldAttr;
    VioSetCurType ( &vioco, hVio );
}

void GetXY ( size_t *x, size_t *y )
{
    VioGetCurPos ( y, x, hVio );
}

void GotoXY ( size_t x, size_t y )
{
    curX = x;
    curY = y;
    VioSetCurPos ( y, x, hVio );
}

UINT32 CurrentTime ()
{
    UINT32 time;
    DosQuerySysInfo ( QSV_MS_COUNT, QSV_MS_COUNT, &time, 4 );
    return time;
}

void PutXY ( size_t x, size_t y, const char *ptr, size_t length )
{
    VioWrtCharStr ( ptr, length, y, x, hVio );
    curX = x + length;
    curY = y;
}

void Put ( const char *ptr, size_t length )
{
    VioWrtCharStr ( ptr, length, curY, curX, hVio );
    curX += length;
}

#elif defined ( OS_LINUX ) || defined ( OS_MACOSX )

static FILE *console;
static termios stored;
static termios term_getch;
static termios term_kbhit;
static UINT32 lastChar;
static int keyhit;
static bool cursor_visible = true;

int cprintf ( const char *fmt, ... )
{
    va_list args;
    va_start ( args, fmt );

    int ret = vfprintf ( console, fmt, args );

    fflush ( console );

    va_end ( args );

    return ret;
}

void SignalHandler ( int signal )
{
    switch ( signal ) {
        case SIGABRT :
            RestoreConsoleSettings ();
            break;
        case SIGSEGV :
            fprintf ( stderr, "Segmentation fault" );
        case SIGINT :
        case SIGTSTP :
            RestoreConsoleSettings ();
            printf ( "\r\n" );
            raise ( signal );
            break;
        case SIGCONT :
            SaveConsoleSettings ();
            break;
    }
}

void SaveConsoleSettings ()
{
    tcgetattr ( 0, &stored );
    memcpy ( &term_getch, &stored, sizeof ( struct termios ));
    // Disable echo
    term_getch.c_lflag    &= ~ECHO;
    // Disable canonical mode, and set buffer size to 1 byte
    term_getch.c_lflag    &= ~ICANON;
    term_getch.c_cc[VTIME] = 0;
    term_getch.c_cc[VMIN]  = 1;

    memcpy ( &term_kbhit, &term_getch, sizeof ( struct termios ));
    term_kbhit.c_cc[VTIME] = 0;
    term_kbhit.c_cc[VMIN]  = 0;

    // Create a 'console' device
    console = fopen ( "/dev/tty", "w" );
    if ( console == NULL ) {
        console = stdout;
    }

    HideCursor ();

    struct sigaction sa;
    memset ( &sa, 0, sizeof ( sa ));
    sa.sa_handler = SignalHandler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset ( &sa.sa_mask );

    sigaction ( SIGINT,  &sa, NULL );
    sigaction ( SIGABRT, &sa, NULL );
    sigaction ( SIGSEGV, &sa, NULL );
    sigaction ( SIGTSTP, &sa, NULL );
    sigaction ( SIGCONT, &sa, NULL );

    atexit ( RestoreConsoleSettings );
}

void RestoreConsoleSettings ()
{
    struct sigaction sa;
    memset ( &sa, 0, sizeof ( sa ));
    sa.sa_handler = SIG_DFL;

    sigaction ( SIGINT,  &sa, NULL );
    sigaction ( SIGABRT, &sa, NULL );
    sigaction ( SIGSEGV, &sa, NULL );
    sigaction ( SIGTSTP, &sa, NULL );
    sigaction ( SIGCONT, &sa, NULL );

    tcsetattr ( 0, TCSANOW, &stored );

    ShowCursor ();

    if ( console != stdout ) {
        fclose ( console );
        console = stdout;
    }
}

void HideCursor ()
{
    if ( cursor_visible == true ) {
        fprintf ( console, "\033[?25l" );
        fflush ( console );
        cursor_visible = false;
    }
}

void ShowCursor ()
{
    if ( cursor_visible == false ) {
        fprintf ( console, "\033[?25h" );
        fflush ( console );
        cursor_visible = true;
    }
}

UINT32 GetKey ()
{
    UINT32 key = lastChar;
    lastChar = 0;

    if ( keyhit == 0 ) {
        tcsetattr ( 0, TCSANOW, &term_getch );
        keyhit = read ( STDIN_FILENO, &key, sizeof ( key ));
    }

    UINT8 *ptr = ( UINT8 * ) &key;
    UINT32 retVal = ( ptr [3] << 24 ) | ( ptr [2] << 16 ) | ( ptr [1] << 8 ) | ptr [0];

    keyhit = 0;

    return retVal;
}

bool KeyPressed ()
{
    if ( keyhit == 0 ) {
        tcsetattr ( 0, TCSANOW, &term_kbhit );
        keyhit = read ( STDIN_FILENO, &lastChar, sizeof ( lastChar ));
    }

    return ( keyhit != 0 ) ? true : false;
}

UINT32 CurrentTime ()
{
    timeval time;
    gettimeofday ( &time, NULL );
    return ( UINT32 ) (( time.tv_sec * 1000 ) + ( time.tv_usec / 1000 ));
}

void ClearScreen ()
{
    GotoXY ( 0, 0 );
    fprintf ( console, "\033[2J" );
    fflush ( console );
}

void SetColor ( bool bold, int foreground, int background )
{
    fprintf ( console, "\033[m" );
    if ( bold ) fprintf ( console, "\033[1m" );
    if ( foreground != COLOR_DEFAULT ) fprintf ( console, "\033[%dm", 30 + foreground );
    if ( background != COLOR_DEFAULT) fprintf ( console, "\033[%dm", 40 + background );
    fflush ( console );
}

void GetXY ( size_t *x, size_t *y )
{
    *x = curX;
    *y = curY;
}

void GotoXY ( size_t x, size_t y )
{
    if (( x + 1 != curX ) || ( y + 1 != curY )) {
        fprintf ( console, "\033[%d;%dH", ( int ) ( y + 1 ), ( int ) ( x + 1 ));
    }
    curX = x + 1;
    curY = y + 1;
}

void PutXY ( size_t x, size_t y, const char *ptr, size_t length )
{
    GotoXY ( x, y );
    Put ( ptr, length );
}

void Put ( const char *ptr, size_t length )
{
    char buffer [80];

    if ( length > 80 ) length = 80;

    for ( size_t i = 0; i < length; i++ ) {
        buffer [i] = (( ptr [i] < 32 ) || ( ptr [i] > 126 )) ? '.' : ptr [i];
    }

    fwrite ( buffer, length, 1, console );
    fflush ( console );

    curX += length;
}

#elif defined ( OS_WINDOWS )

#if defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )
    #define cprintf _cprintf
#endif

#define MAX_SCREEN_WIDTH          132

static CONSOLE_SCREEN_BUFFER_INFO screenInfo;
static CONSOLE_CURSOR_INFO        cursorInfo;
static BOOL                       oldVisible;

static HANDLE                     hOutput;
static COORD                      currentPos;

static LARGE_INTEGER              timerFrequency;
static WORD                       Attributes [MAX_SCREEN_WIDTH];

long WINAPI myHandler ( PEXCEPTION_POINTERS )
{
    RestoreConsoleSettings ();

    return EXCEPTION_CONTINUE_SEARCH;
}

void SignalHandler ( int )
{
    RestoreConsoleSettings ();
    cprintf ( "\r\n" );
    exit ( -1 );
}

void SaveConsoleSettings ()
{
    hOutput = GetStdHandle ( STD_OUTPUT_HANDLE );
    GetConsoleCursorInfo ( hOutput, &cursorInfo );
    GetConsoleScreenBufferInfo ( hOutput, &screenInfo );
    oldVisible = cursorInfo.bVisible;
    SetUnhandledExceptionFilter ( myHandler );
    signal ( SIGBREAK, SignalHandler );
    signal ( SIGINT, SignalHandler );
    atexit ( RestoreConsoleSettings );

    QueryPerformanceFrequency ( &timerFrequency );
}

void RestoreConsoleSettings ()
{
    cursorInfo.bVisible = oldVisible;
    SetConsoleCursorInfo ( hOutput, &cursorInfo );
}

void HideCursor ()
{
    CONSOLE_CURSOR_INFO info = cursorInfo;
    info.bVisible = FALSE;
    SetConsoleCursorInfo ( hOutput, &info );
}

void ShowCursor ()
{
    CONSOLE_CURSOR_INFO info = cursorInfo;
    info.bVisible = TRUE;
    SetConsoleCursorInfo ( hOutput, &info );
}

UINT32 GetKey ()
{
    UINT32 key = 0;
    UINT8 *ptr = ( UINT8 * ) &key;
    int ch = getch ();

    // "Eat" F1-F10, Home, End, Home, End, Delete, Insert, PgUp, PgDown, PageUp, PageDown, and the arrow keys
    if ( ch == 0x00 ) {
        ch = getch ();
        return 0;
    }

    if ( ch == 0xE0 ) {
        *ptr++ = 0x1B;
        *ptr++ = 0x5B;
        ch = getch ();
        switch ( ch ) {
            case 0x48 : ch = 0x41;  break;
            case 0x50 : ch = 0x42;  break;
            case 0x4D : ch = 0x43;  break;
            case 0x4B : ch = 0x44;  break;
        }
    }
    *ptr++ = ( UINT8 ) ch;

    return key;
}

bool KeyPressed ()
{
    return ( kbhit () != 0 ) ? true : false;
}

UINT32 CurrentTime ()
{
    LARGE_INTEGER time;
    QueryPerformanceCounter ( &time );

    return ( UINT32 ) ( 1000 * time.QuadPart / timerFrequency.QuadPart );
}

void ClearScreen ()
{
    DWORD dwActual;
    COORD origin;
    origin.X = 0;
    origin.Y = 0;
    FillConsoleOutputCharacter ( hOutput, ' ', screenInfo.dwSize.X * screenInfo.dwSize.Y, origin, &dwActual );
    FillConsoleOutputAttribute ( hOutput, Attributes[0], screenInfo.dwSize.X * screenInfo.dwSize.Y, origin, &dwActual );

    curX = screenInfo.dwCursorPosition.X = 0;
    curY = screenInfo.dwCursorPosition.Y = 0;
}

void SetColor ( bool bold, int foreground, int background )
{
    WORD attr = bold ? FOREGROUND_INTENSITY : 0;

    if ( foreground == COLOR_DEFAULT ) foreground = COLOR_WHITE;
    if ( background == COLOR_DEFAULT ) background = COLOR_BLACK;

    if ( foreground & COLOR_RED )   attr |= FOREGROUND_RED;
    if ( foreground & COLOR_GREEN ) attr |= FOREGROUND_GREEN;
    if ( foreground & COLOR_BLUE )  attr |= FOREGROUND_BLUE;
    if ( background & COLOR_RED )   attr |= BACKGROUND_RED;
    if ( background & COLOR_GREEN ) attr |= BACKGROUND_GREEN;
    if ( background & COLOR_BLUE )  attr |= BACKGROUND_BLUE;

    for ( int i = 0; i < MAX_SCREEN_WIDTH; i++ ) {
        Attributes [i] = attr;
    }
}

void GetXY ( size_t *x, size_t *y )
{
    GetConsoleScreenBufferInfo ( hOutput, &screenInfo );
    *x = screenInfo.dwCursorPosition.X;
    *y = screenInfo.dwCursorPosition.Y;
}

void GotoXY ( size_t x, size_t y )
{
    curX = x;
    curY = y;
    if ( cursorInfo.bVisible == TRUE ) {
        COORD cursor;
        cursor.X = ( SHORT ) curX;
        cursor.Y = ( SHORT ) curY;
        SetConsoleCursorPosition ( hOutput, cursor );
    }
}

void PutXY ( size_t x, size_t y, const char *ptr, size_t length )
{
    DWORD count;
    currentPos.X = ( SHORT ) x;
    currentPos.Y = ( SHORT ) y;
    WriteConsoleOutputCharacter ( hOutput, ptr, ( DWORD ) length, currentPos, &count );
    WriteConsoleOutputAttribute ( hOutput, Attributes, ( DWORD ) length, currentPos, &count );
    curX = x + length;
    curY = y;
}

void Put ( const char *ptr, size_t length )
{
    DWORD count;
    currentPos.X = ( SHORT ) curX;
    currentPos.Y = ( SHORT ) curY;
    WriteConsoleOutputCharacter ( hOutput, ptr, ( DWORD ) length, currentPos, &count );
    WriteConsoleOutputAttribute ( hOutput, Attributes, ( DWORD ) length, currentPos, &count );
    curX += length;
}

#endif

void outByte ( UINT8 val )
{
    char buffer [ 3 ], *ptr = &buffer[3], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex ( val );
    Put ( ptr, 2 );
}

void outWord ( UINT16 val )
{
    char buffer [5], *ptr = &buffer[5], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex (( char ) val );
    Put ( ptr, 4 );
}

void outLong ( UINT32 val )
{
    char buffer [9], *ptr = &buffer[9], digit;
    *--ptr = '\0';
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    digit = ( char ) ( val & 0x0F );    val >>= 4;
    *--ptr = toHex ( digit );
    *--ptr = toHex (( char ) val );
    Put ( ptr, 8 );
}
