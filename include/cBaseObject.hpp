//----------------------------------------------------------------------------
//
// File:	cBaseObject.hpp
// Name:	
// Programmer:	Marc Rousseau
// Date:      	13-April-1998
//
// Description:	
//
// Revision History:
//
//----------------------------------------------------------------------------

#ifndef CBASEOBJECT_HPP_
#define CBASEOBJECT_HPP_

#include "iBaseObject.hpp"

#include <vector>
#include <map>

#define BASE_OBJECT_VALID		0x4556494C	// "LIVE"
#define BASE_OBJECT_INVALID		0x44414544	// "DEAD"

class cBaseObject : virtual public iBaseObject {

#ifdef DEBUG

public:

    static void Check ();

protected:

    typedef std::vector<cBaseObject *> tBaseObjectList;
    typedef std::map<cBaseObject *,cBaseObject *> tBaseObjectMap;

    static tBaseObjectMap sm_ActiveObjects;

    UINT32              m_Tag;
    const char         *m_ObjectName;		// Name of most-derived Class
    int                 m_ObjectCount;		// # of objects referenced by this object
    tBaseObjectList     m_OwnerList;

    const char *ObjectName () const;

    void AddObject ();
    void RemoveObject ();

    void AddOwner ( cBaseObject * );
    void RemoveOwner ( cBaseObject * );

    virtual bool CheckValid () const;
    void DumpObject ( const char * ) const;

#endif

    int                 m_RefCount;		// # of references on this object

private:

    // Disable the copy constructor and assignment operator defaults
    cBaseObject ();					// no implementation
    cBaseObject ( const cBaseObject & );		// no implementation
    void operator = ( const cBaseObject & );		// no implementation

protected:

    // We don't want anyone creating/deleting these directly - all derived classes should do the same
    cBaseObject ( const char *name );
    virtual ~cBaseObject ();

public:

    // iBaseObject Methods
    bool GetInterface ( const char *, iBaseObject ** );
    int  AddRef ( iBaseObject * );
    int  Release ( iBaseObject * );

#ifdef DEBUG

    // NOTE: This function *CANNOT* be virtual!
    bool IsValid () const;

#endif

};

#endif
