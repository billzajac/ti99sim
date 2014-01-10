//----------------------------------------------------------------------------
//
// File:        logger.hpp
// Date:        15-Apr-1998
// Programmer:  Marc Rousseau
//
// Description: Error Logger object header
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

#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <string>

#if defined ( LOGGER )

    struct LOG_FLAGS {
        bool    FunctionEntry;
    };

    extern LOG_FLAGS g_LogFlags;

    #define FUNCTION_ENTRY(t,f,l)						\
        const void * const l_pThis  = static_cast<const void * const>( t );		\
        const char * const l_FnName = f;					\
        bool const l_log = ( g_LogFlags.FunctionEntry == l ) ? true : false;	\
        if ( l_log && g_LogFlags.FunctionEntry ) {				\
            DBG_TRACE ( "Function entered" );					\
        }

#else

    #define FUNCTION_ENTRY(t,f,l)

#endif

#if defined ( LOGGER ) && defined ( DEBUG )

    #define DBG_ASSERT(x)          (( ! ( x )) ? dbg_Assert ( dbg_FileName, __LINE__, l_FnName, l_pThis, #x ) : 0 )
    #define DBG_ASSERT_BOOL(x)     DBG_ASSERT ( x )

#else

    #if defined ( _MSC_VER )
        #pragma warning ( disable: 4127 )    // C4127: conditional expression is constant
    #endif

    #define DBG_ASSERT(x)
    #define DBG_ASSERT_BOOL(x)     0

#endif

#if ! defined ( LOGGER )

    #define DBG_REGISTER(x)        class dbgString
    #define DBG_STRING(x)          ""
    #define DBG_TRACE(x)
    #define DBG_TRACE2(x)
    #define DBG_STATUS(x)
    #define DBG_STATUS2(x)
    #define DBG_EVENT(x)
    #define DBG_EVENT2(x)
    #define DBG_WARNING(x)
    #define DBG_WARNING2(x)
    #define DBG_ERROR(x)
    #define DBG_ERROR2(x)
    #define DBG_FATAL(x)
    #define DBG_FATAL2(x)

#else

    #define DBG_REGISTER(x)        static int dbg_FileName = dbg_RegisterFile ( x )
    #define DBG_STRING(x)          x
    #define DBG_TRACEx(x)          dbg_Record ( _TRACE_,       dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_TRACE(x)           dbg_Record ( _TRACE_,       dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )
    #define DBG_STATUSx(x)         dbg_Record ( _STATUS_,      dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_STATUS(x)          dbg_Record ( _STATUS_,      dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )
    #define DBG_EVENTx(x)          dbg_Record ( _EVENT_,       dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_EVENT(x)           dbg_Record ( _EVENT_,       dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )
    #define DBG_WARNINGx(x)        dbg_Record ( _WARNING_,     dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_WARNING(x)         dbg_Record ( _WARNING_,     dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )
    #define DBG_ERRORx(x)          dbg_Record ( _ERROR_,       dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_ERROR(x)           dbg_Record ( _ERROR_,       dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )
    #define DBG_FATALx(x)          dbg_Record ( _FATAL_,       dbg_FileName, __LINE__, l_FnName, l_pThis, x )
    #define DBG_FATAL(x)           dbg_Record ( _FATAL_,       dbg_FileName, __LINE__, l_FnName, l_pThis, dbg_Stream () << x )

    enum eLOG_TYPE {
        _TRACE_,
        _STATUS_,
        _EVENT_,
        _WARNING_,
        _ERROR_,
        _FATAL_,
        LOG_TYPE_MAX
    };  

    class dbgString {

        friend dbgString &hex ( dbgString & );
        friend dbgString &dec ( dbgString & );

        int     m_Base;
        char   *m_Ptr;
        char   *m_Buffer;

    public:

        dbgString ( char * );

        operator const char * () const;
        void flush ();

        dbgString &operator << ( const std::string & );
        dbgString &operator << ( const char * );
        dbgString &operator << ( char );
        dbgString &operator << ( unsigned char );
        dbgString &operator << ( short );
        dbgString &operator << ( unsigned short );
        dbgString &operator << ( int );
        dbgString &operator << ( unsigned int );
        dbgString &operator << ( long );
        dbgString &operator << ( unsigned long );
/*
        dbgString &operator << ( __int64 );
        dbgString &operator << ( unsigned __int64 );
*/
        dbgString &operator << ( double );
        dbgString &operator << ( void * );
        dbgString &operator << ( dbgString &(*f) ( dbgString & ));

    };

    dbgString &hex ( dbgString & );
    dbgString &dec ( dbgString & );

    dbgString  &dbg_Stream ();
    int         dbg_RegisterFile ( const char * );
    int         dbg_Assert ( int, int, const char * , const void *, const char * );
    void        dbg_Record ( int, int, int, const char *, const void *, const char * );
    void        dbg_Record ( int, int, int, const char *, const void *, dbgString & );

#endif

#endif
