//----------------------------------------------------------------------------
//
// File:        device.hpp
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

#ifndef DEVICE_HPP_
#define DEVICE_HPP_

#include "tms9900.hpp"

class cTMS9900;
class cCartridge;

class cDevice {

protected:

    bool        m_IsValid;
    int         m_CRU;
    cCartridge *m_pROM;
    cTMS9900   *m_pCPU;

public:

    cDevice ( const char *filename );
    virtual ~cDevice ();

    void SetCPU ( cTMS9900 *cpu )	{ m_pCPU = cpu; }

    int GetCRU () const			{ return m_CRU; }
    cCartridge *GetROM () const		{ return m_pROM; }

    virtual void Activate ()		{}
    virtual void DeActivate ()		{}

    virtual void WriteCRU ( ADDRESS, int ) = 0;
    virtual int  ReadCRU ( ADDRESS ) = 0;

    virtual bool LoadImage ( FILE * ) = 0;
    virtual bool SaveImage ( FILE * ) = 0;

private:

    cDevice ( const cDevice & );         // no implementation
    void operator = ( const cDevice & ); // no implementation

};

#endif
