//----------------------------------------------------------------------------
//
// File:        device.cpp
// Date:        27-Mar-1998
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

#include <stdio.h>
#include "common.hpp"
#include "logger.hpp"
#include "cartridge.hpp"
#include "tms9900.hpp"
#include "device.hpp"

DBG_REGISTER ( __FILE__ );

cDevice::cDevice ( const char *filename ) :
    m_IsValid ( true ),
    m_CRU ( -1 ),
    m_pROM ( NULL ),
    m_pCPU ( NULL )
{
    FUNCTION_ENTRY ( this, "cDevice ctor", true );

    if ( filename != NULL ) {
        m_pROM = new cCartridge ( filename );
        if ( ! m_pROM->IsValid () ||
            ( m_pROM->CpuMemory[4].Bank[0].Data == NULL ) ||
            ( m_pROM->CpuMemory[5].Bank[0].Data == NULL )) {
            DBG_ERROR ( "Cartridge does not appear to be a valid device" );
            m_IsValid = false;
        } else {
            m_CRU  = m_pROM->GetCRU ();
        }
    }
}

cDevice::~cDevice ()
{
    FUNCTION_ENTRY ( this, "cDevice dtor", true );

    delete m_pROM;
    m_pROM = NULL;
}
