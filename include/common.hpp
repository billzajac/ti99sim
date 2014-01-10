//----------------------------------------------------------------------------
//
// File:	common.hpp
// Date:
// Programmer:	Marc Rousseau
//
// Description:
//
// Copyright (c) 1994-2004 Marc Rousseau, All Rights Reserved.
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

#ifndef COMMON_HPP_
#define COMMON_HPP_

//----------------------------------------------------------------------------
// Determine platform & OS
//----------------------------------------------------------------------------

#include "platform.hpp"

//----------------------------------------------------------------------------
// Generic definitions
//----------------------------------------------------------------------------

#ifdef UNREFERENCED_PARAMETER
    #undef UNREFERENCED_PARAMETER
#endif

#if defined ( _MSC_VER )
    #define UNREFERENCED_PARAMETER(x)	( void ) x

    #pragma warning ( disable : 4996 )  // 'function' was declared deprecated
    #pragma warning ( disable : 4351 )  // Tell MSVC to stop warning me I'm using C++ features

    #define snprintf _snprintf
#else
    #define UNREFERENCED_PARAMETER(x)
#endif

template <typename T, size_t N> char ( &ArraySizeHelper ( T (&array) [N] )) [N];

#define OFFSET_OF(t,x)	(( size_t ) & (( t * ) 0 )->( x ))
#define SIZE(x)		( sizeof ( ArraySizeHelper ( x )))
#define EVER		;;

#if defined( _MSC_VER )
	#include <basetsd.h>
	/*
    typedef signed char        INT8;
    typedef signed short       INT16;
    typedef signed int         INT32;
    typedef signed long long   INT64;
    typedef unsigned char      UINT8;
    typedef unsigned short     UINT16;
    typedef unsigned int       UINT32;
    typedef unsigned long long UINT64;
	*/
#else
    #include <stdint.h>
    typedef int8_t             INT8;
    typedef int16_t            INT16;
    typedef int32_t            INT32;
    typedef int64_t            INT64;
    typedef uint8_t            UINT8;
    typedef uint16_t           UINT16;
    typedef uint32_t           UINT32;
    typedef uint64_t           UINT64;
#endif

#if defined ( __GNUC__ )

    #if defined( __cplusplus )
    	#define restrict	__restrict__	
    #endif

#else

    #define restrict

#endif

#if defined ( OS_WINDOWS ) || defined ( OS_AMIGAOS )

    // Undo any previous definitions
    #define BIG_ENDIAN      1234
    #define LITTLE_ENDIAN   4321

    #if defined ( OS_WINDOWS ) || defined ( OS_AMIGAOS )

        #define BYTE_ORDER      LITTLE_ENDIAN

    #else

        #define BYTE_ORDER      BIG_ENDIAN

    #endif

#else

    // Use the environments endian definitions
    #undef  __USE_BSD
    #define __USE_BSD
    #include <endian.h>

#endif

typedef int (*QSORT_FUNC) ( const void *, const void * );

#ifdef max
    #undef max
#endif
#ifdef min
    #undef min
#endif

//----------------------------------------------------------------------------
// Platform specific definitions
//----------------------------------------------------------------------------

#if defined ( OS_OS2 ) || defined ( OS_WINDOWS )

    #define SEPERATOR	'\\'
    #define DEFAULT_CHAR	"û"		// DOS CP-?

#elif defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )

    #define SEPERATOR	'/'
    #define DEFAULT_CHAR	"✓"		// UTF-8 0x2713 - Checkmark
    
#else

    #define SEPERATOR	'/'
    #define DEFAULT_CHAR	"*"		// ASCII safe asterisk

#endif

//----------------------------------------------------------------------------
// Compiler specific definitions
//----------------------------------------------------------------------------

#if defined ( __BORLANDC__ )

    #if defined ( __BCPLUSPLUS__ ) || defined ( __TCPLUSPLUS__ )
        #undef  NULL
        #define NULL ( void * ) 0
    #endif

    #if (( __BORLANDC__ < 0x0500 ) || defined ( OS_OS2 ))

        // Fake out ANSI definitions for deficient compilers
        #define for    if (0); else for

        enum { false, true };
        class bool {
            int   value;
        public:
            operator = ( int x ) { value = x ? true : false; }
            bool operator ! ()   { return value; }
        };

    #endif

    template <class T> inline const T &min ( const T &t1, const T &t2 )
    {
        return ( t1 < t2 ) ? t1 : t2;
    }

    template <class T> inline const T &max ( const T &t1, const T &t2 )
    {
        return ( t1 > t2 ) ? t1 : t2;
    }

#elif defined ( _MSC_VER )

    #undef  NULL
    #define NULL        0L

    #define MAXPATH	_MAX_PATH
    #define MAXDRIVE	_MAX_DRIVE
    #define MAXDIR	_MAX_DIR
    #define MAXFNAME	_MAX_FNAME
    #define MAXEXT	_MAX_EXT

    #if ( _MSC_VER < 1300 )

        // Fake out ANSI definitions for deficient compilers
        #define for    if (0); else for

        #pragma warning ( disable: 4127 )    // C4127: conditional expression is constant

    #endif

    #pragma warning ( disable: 4244 )    // C4244: 'xyz' conversion from 'xxx' to 'yyy', possible loss of data
    #pragma warning ( disable: 4290 )    // C4290: C++ exception specification ignored...
    #pragma warning ( disable: 4514 )    // C4514: 'xyz' unreferenced inline function has been removed

    template <class T> inline const T &min ( const T &t1, const T &t2 )
    {
        return ( t1 < t2 ) ? t1 : t2;
    }

    template <class T> inline const T &max ( const T &t1, const T &t2 )
    {
        return ( t1 > t2 ) ? t1 : t2;
    }

    #if ( _MSC_VER >= 1300 )
        extern char *strndup ( const char *string, size_t max );
    #endif

#elif defined ( __WATCOMC__ )

    #define M_PI        3.14159265358979323846

    template <class T> inline const T &min ( const T &t1, const T &t2 )
    {
        return ( t1 < t2 ) ? t1 : t2;
    }

    template <class T> inline const T &max ( const T &t1, const T &t2 )
    {
        return ( t1 > t2 ) ? t1 : t2;
    }

#elif defined ( __GNUC__ ) || defined ( __INTEL_COMPILER )

    #define MAXPATH	FILENAME_MAX
    #define MAXDRIVE	FILENAME_MAX
    #define MAXDIR	FILENAME_MAX
    #define MAXFNAME	FILENAME_MAX
    #define MAXEXT	FILENAME_MAX

    #define O_BINARY	0

    #define stricmp strcasecmp
    #define strnicmp strncasecmp

        template <class T> inline const T &min ( const T &t1, const T &t2 )
        {
        return ( t1 < t2 ) ? t1 : t2;
        }

        template <class T> inline const T &max ( const T &t1, const T &t2 )
        {
            return ( t1 > t2 ) ? t1 : t2;
        }

#elif defined ( OS_AMIGAOS )

    // Fake out ANSI definitions for deficient compilers
    #define for    if (0); else for

    enum { false, true };
    class bool {
        int   value;
    public:
        bool ( int x = 0 )   { value = x ? true : false; }
        bool &operator = ( int x ) { value = x ? true : false; return *this; }
        bool operator ! ()   { return value; }
    };

    template <class T> inline const T &min ( const T &t1, const T &t2 )
    {
        return ( t1 < t2 ) ? t1 : t2;
    }

    template <class T> inline const T &max ( const T &t1, const T &t2 )
    {
        return ( t1 > t2 ) ? t1 : t2;
    }

    extern char *strdup ( const char *string );
    extern char *strndup ( const char *string, size_t max );

#endif

#endif
