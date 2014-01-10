//----------------------------------------------------------------------------
//
// File:        support.cpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description:
//
// Copyright (c) 2003-2004 Marc Rousseau, All Rights Reserved.
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
//   15-Jan-2003    Renamed from original fileio.cpp
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined ( __GNUC__ )
    #include <unistd.h>
#endif
#if defined ( OS_WINDOWS )
    #include <windows.h>
#endif
#include "common.hpp"
#include "logger.hpp"
#include "support.hpp"

DBG_REGISTER ( __FILE__ );

#if defined ( OS_LINUX ) || defined ( OS_MACOSX )
    #define MKDIR(x,y) mkdir ( x, y )
    const char FILE_SEPERATOR = '/';
    const char *COMMON_PATH = "/opt/ti99sim";
#elif defined ( OS_OS2 ) || defined ( OS_WINDOWS )
    #include <direct.h>
    #define MKDIR(x,y) mkdir ( x )
    const char FILE_SEPERATOR = '\\';
    const char *COMMON_PATH = "";
#elif defined ( OS_AMIGAOS )
    #include <direct.h>
    #define MKDIR(x,y) mkdir ( x )
    const char FILE_SEPERATOR = '/';
    const char *COMMON_PATH = "/opt/ti99sim";
#endif

static char home [ 256 ] = ".";

static int CreateHomePath ()
{
    FUNCTION_ENTRY ( NULL, "CreateHomePath", true );

    const char *ptr = getenv ( "HOME" );

#if defined ( OS_WINDOWS )
    char fileName [ 256 ];
    if ( ptr == NULL ) {
        GetModuleFileName ( NULL, fileName, 256 );
        char *end = strrchr ( fileName, FILE_SEPERATOR );
        if ( end != NULL ) *end = '\0';
        ptr = fileName;
    }
#endif

    sprintf ( home, "%s%c.ti99sim", ptr ? ptr : ".", FILE_SEPERATOR );
    MKDIR ( home, 0775 );

    return 0;
}

static int x = CreateHomePath ();

const char *HOME_PATH = home;

bool IsWriteable ( const char *filename )
{
    FUNCTION_ENTRY ( NULL, "IsWriteable", true );

    bool retVal = false;

    struct stat info;
    if ( stat ( filename, &info ) == 0 ) {
#if defined ( OS_WINDOWS ) || defined ( OS_AMIGAOS )
        if ( info.st_mode & S_IWRITE ) retVal = true;
#else
        if ( getuid () == info.st_uid ) {
            if ( info.st_mode & S_IWUSR ) retVal = true;
        } else if ( getgid () == info.st_gid ) {
            if ( info.st_mode & S_IWGRP ) retVal = true;
        } else {
            if ( info.st_mode & S_IWOTH ) retVal = true;
        }
#endif
    } else {
        // TBD: Check write permissions to the directory
        retVal = true;
    }

    return retVal;
}

static bool TryPath ( const char *path, const char *filename )
{
    FUNCTION_ENTRY ( NULL, "TryPath", true );

    char buffer [256];

    sprintf ( buffer, "%s%c%s", path, FILE_SEPERATOR, filename );

    DBG_TRACE ( "Name: " << buffer );

    FILE *file = fopen ( buffer, "rb" );
    if ( file != NULL ) {
        fclose ( file );
        return true;
    }

    return false;
}

const char *LocateFile ( const char *filename, const char *path )
{
    FUNCTION_ENTRY ( NULL, "LocateFile", true );

    static char buffer [256];
    static char fullname [256];

    if ( filename == NULL ) return NULL;

    // If we were given an absolute path, see if it's valid or not
#if defined ( OS_WINDOWS )
    if (( filename [0] == FILE_SEPERATOR ) || ( filename [1] == ':' )) {
#else
    if ( filename [0] == FILE_SEPERATOR ) {
#endif
        FILE *file = fopen ( filename, "rb" );
        if ( file != NULL ) {
            fclose ( file );
            return filename;
        }
        return NULL;
    }

    // Make a static copy of filename
    strcpy ( buffer, filename );

    if ( path == NULL ) {
        strcpy ( fullname, filename );
    } else {
        sprintf ( fullname, "%s%c%s", path, FILE_SEPERATOR, filename );
    }

    const char *pPath = NULL;
    const char *pFile = NULL;

    // Try: CWD, CWD/path, ~/.ti99sim/path, /opt/ti99sim/path
    if (( TryPath ( ".", pFile = buffer )                   == true ) ||
        ( TryPath ( ".", pFile = fullname )                 == true ) ||
        ( TryPath ( pPath = home, pFile = fullname )        == true ) ||
        ( TryPath ( pPath = COMMON_PATH, pFile = fullname ) == true )) {
        if ( pPath == NULL ) return pFile;
        sprintf ( buffer, "%s%c%s", pPath, FILE_SEPERATOR, pFile );
        return buffer;
    }

    return NULL;
}

#if defined ( OS_AMIGAOS )

char *strdup ( const char *string )
{
    size_t length = strlen ( string );

    char *ptr = ( char * ) malloc ( length + 1 );
    if ( ptr != NULL ) {
        memcpy ( ptr, string, length );
        ptr [length] = '\0';
    }

    return ptr;
}

#endif

#if defined ( OS_AMIGAOS ) || defined ( _MSC_VER )

char *strndup ( const char *string, size_t max )
{
    size_t length = strlen ( string );
    if ( length > max ) length = max;

    char *ptr = ( char * ) malloc ( length + 1 );
    if ( ptr != NULL ) {
        memcpy ( ptr, string, length );
        ptr [length] = '\0';
    }

    return ptr;
}

#endif
