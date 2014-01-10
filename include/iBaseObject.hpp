//----------------------------------------------------------------------------
//
// File:	iBaseObject.hpp
// Name:	
// Programmer:	Marc Rousseau
// Date:      	13-April-1998
//
// Description:	
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef IBASEOBJ_HPP_
#define IBASEOBJ_HPP_

#if defined ( _MSC_VER )
   #pragma warning ( disable: 4250 )    // 'class1' : inherits 'class2::member' via dominance
#endif

struct iBaseObject {

    virtual bool GetInterface ( const char *, iBaseObject ** ) = 0;
    virtual int  AddRef ( iBaseObject * ) = 0;
    virtual int  Release ( iBaseObject * ) = 0;

protected:

    virtual ~iBaseObject () {}

};

#endif
