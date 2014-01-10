//----------------------------------------------------------------------------
//
// File:		cBaseObject.cpp
// Name:
// Programmer:	Marc Rousseau
// Date:      	13-April-1998
//
// Description:
//
// Revision History:
//
//----------------------------------------------------------------------------

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <typeinfo>

#include "common.hpp"
#include "logger.hpp"
#include "iBaseObject.hpp"
#include "cBaseObject.hpp"

DBG_REGISTER ( __FILE__ );

#ifdef DEBUG

    cBaseObject::tBaseObjectMap cBaseObject::sm_ActiveObjects;

    void cBaseObject::Check ()
    {
        FUNCTION_ENTRY ( NULL, "Check", true );

        size_t count = sm_ActiveObjects.size ();

        if ( count != 0 ) {
            DBG_ERROR ( "There " << (( count == 1 ) ? "is" : "are" ) << " still " << count << " active cBaseObject" << (( count != 1 ) ? "s" : "" ));
            tBaseObjectMap::iterator iter = sm_ActiveObjects.begin ();
            while ( iter != sm_ActiveObjects.end ()) {
                (*iter).second->DumpObject ( "  " );
                iter++;
            }
        }
    }

    static int x = atexit ( cBaseObject::Check );

#endif

cBaseObject::cBaseObject ( const char *name ) :
#ifdef DEBUG
    m_Tag ( BASE_OBJECT_VALID ),
    m_ObjectName ( name ),
    m_ObjectCount ( 0 ),
    m_OwnerList( ),
#endif
    m_RefCount ( 1 )
{
    FUNCTION_ENTRY ( this, "cBaseObject ctor", true );

    DBG_TRACE ( "New " << name << " object: " << this << " (" << ( iBaseObject * ) this << ")" );

#ifdef DEBUG
    DBG_ASSERT ( name != NULL );

    AddOwner ( NULL );

    sm_ActiveObjects[this] = this;
#else
    UNREFERENCED_PARAMETER ( name );
#endif
}

cBaseObject::~cBaseObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject dtor", true );

#ifdef DEBUG

    if ( DBG_ASSERT_BOOL ( IsValid ())) return;

    // We should only get here through Release, but make sure anyway
    DBG_ASSERT ( m_RefCount == 0 );

    // RefCount should match the OwnerList
    if ( m_RefCount != ( int ) m_OwnerList.size ()) {
        DBG_ERROR ( m_ObjectName << " object " << ( void * ) this << " RefCount discrepancy (" << m_RefCount << " vs " << m_OwnerList.size () << ")" );
    }

    if (( m_OwnerList.empty () == false ) || ( m_ObjectCount > 0 )) {
        DumpObject ( "" );
    }

    m_Tag         = BASE_OBJECT_INVALID;
    m_ObjectName  = "<Deleted>";
    m_ObjectCount = 0;

    sm_ActiveObjects.erase ( this );

#endif

    m_RefCount    = 0;

}

#ifdef DEBUG

void cBaseObject::DumpObject ( const char *space ) const
{
    FUNCTION_ENTRY ( this, "cBaseObject::DumpObject", true );

    DBG_TRACE ( space << m_ObjectName << " object " << ( void * ) this );

    if ( m_ObjectCount > 0 ) {
        DBG_TRACE ( space << "  Owns " << m_ObjectCount << " object" << (( m_ObjectCount != 1 ) ? "s" : "" ));
    }

    if ( m_OwnerList.empty () == false ) {
        DBG_TRACE ( space << "  Owned by " << m_OwnerList.size () << (( m_OwnerList.size () == 1 ) ? " object" : " objects" ));
        tBaseObjectList::const_iterator iter = m_OwnerList.begin ();
        while ( iter != m_OwnerList.end ()) {
            DBG_TRACE ( space << "    " << ( (*iter) ? (*iter)->ObjectName () : "NULL" ) << " object " << ( void * ) (*iter));
            iter++;
        }
    }
}

bool cBaseObject::CheckValid () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::CheckValid", true );

    // We don't do anything more here - let derived classes to their thing
    return true;
}

bool cBaseObject::IsValid () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::IsValid", true );

    // Wrap all member variable access inside the excpetion handler
    try {
        // See if we have a valid tag
        if ( m_Tag != BASE_OBJECT_VALID ) return false;

        // OK, it looks like one of ours, try using the virtual CheckValid method
        return CheckValid ();
    }
    catch ( ... ) {
    }

    return false;
}

void cBaseObject::AddObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddObject", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return;

    m_ObjectCount++;
}

void cBaseObject::RemoveObject ()
{
    FUNCTION_ENTRY ( this, "cBaseObject::RemoveObject", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return;
    if ( DBG_ASSERT_BOOL ( m_ObjectCount >= 0 )) return;

    m_ObjectCount--;
}

void cBaseObject::AddOwner ( cBaseObject *owner )
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddOwner", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return;

    DBG_TRACE ( "Adding " << ( void * ) owner << " to owner list for " << ( void * ) this );

    m_OwnerList.push_back ( owner );

    if ( owner != NULL ) owner->AddObject ();
}

void cBaseObject::RemoveOwner ( cBaseObject *owner )
{
    FUNCTION_ENTRY ( this, "cBaseObject::RemoveOwner", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return;

    tBaseObjectList::iterator ptr = std::find ( m_OwnerList.begin (), m_OwnerList.end (), owner );

    if ( ptr != m_OwnerList.end ()) {
        DBG_TRACE ( "Removing " << ( void * ) owner << " from owner list for " << ( void * ) this );
        if ( owner != NULL ) owner->RemoveObject ();
        m_OwnerList.erase ( ptr );
        return;
    }

    DBG_ERROR (( owner ? owner->ObjectName () : "NULL" ) << " object " << ( void * ) owner << " does not own " << m_ObjectName << " object " << ( void * ) this );
}

const char *cBaseObject::ObjectName () const
{
    FUNCTION_ENTRY ( this, "cBaseObject::ObjectName", true );

    return m_ObjectName;
}

#endif

bool cBaseObject::GetInterface ( const char *iName, iBaseObject **pObject )
{
    FUNCTION_ENTRY ( this, "cBaseObject::GetInterface", true );

    if ( strcmp ( iName, "iBaseObject" ) == 0 ) {
        *pObject = ( iBaseObject * ) this;
        return true;
    }

    return false;
}

int cBaseObject::AddRef ( iBaseObject *obj )
{
    FUNCTION_ENTRY ( this, "cBaseObject::AddRef", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return -1;
    if ( DBG_ASSERT_BOOL ( m_RefCount >= 0 )) return -1;

#ifdef DEBUG
    try {
        cBaseObject *base = dynamic_cast <cBaseObject *> ( obj );
        AddOwner ( base );
    }
    catch ( const std::bad_cast &er ) {
        DBG_FATAL ( "Invalid cast: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( const std::exception &er ) {
        DBG_FATAL ( "Exception: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( ... ) {
        DBG_FATAL ( "Unexpected C++ exception!" );
    }
#else
    UNREFERENCED_PARAMETER ( obj );
#endif

    return ++m_RefCount;
}

int cBaseObject::Release ( iBaseObject *obj )
{
    FUNCTION_ENTRY ( this, "cBaseObject::Release", true );

    if ( DBG_ASSERT_BOOL ( IsValid ())) return -1;
    if ( DBG_ASSERT_BOOL ( m_RefCount > 0 )) return -1;

#ifdef DEBUG
    try {
        cBaseObject *base = dynamic_cast <cBaseObject *> ( obj );
        RemoveOwner ( base );
    }
    catch ( const std::bad_cast &er ) {
        DBG_FATAL ( "Invalid cast: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( const std::exception &er ) {
        DBG_FATAL ( "Exception: Error casting " << typeid ( obj ).name () << " (" << obj << ") : " << er.what ());
    }
    catch ( ... ) {
        DBG_FATAL ( "Unexpected C++ exception!" );
    }
#else
    UNREFERENCED_PARAMETER ( obj );
#endif

    int count = --m_RefCount;

    if ( count == 0 ) {
        try {
            delete this;
        }
        catch ( ... ) {
            DBG_FATAL ( "Unexpected C++ exception!" );
        }
        return 0;
    }

    return count;
}
