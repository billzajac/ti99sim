//----------------------------------------------------------------------------
//
// File:        support.hpp
// Date:        15-Jan-2003
// Programmer:  Marc Rousseau
//
// Description: This file contains startup code for Linux/SDL
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

#ifndef SUPPORT_HPP_
#define SUPPORT_HPP_

extern const char FILE_SEPERATOR;
extern const char *HOME_PATH;
extern const char *COMMON_PATH;

bool IsWriteable ( const char *filename );
const char *LocateFile ( const char *filename, const char *path = NULL );

#if defined ( OS_AMIGAOS )
    char *strdup ( const char *string );
#endif

#if defined ( OS_AMIGAOS ) || defined ( _MSC_VER )
    char *strndup ( const char *string, size_t max );
#endif

#endif
