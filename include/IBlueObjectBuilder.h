// Copyright © 2014 CCP ehf.

#pragma once

#ifndef IBLUEOBJECTBUILDER_H
#define IBLUEOBJECTBUILDER_H

BLUE_INTERFACE( IBlueObjectBuilder ) : public IRoot
{
	// Create the object from the objectMarker. This can possibly yield the calling tasklet.
	virtual IRoot* CreateObjectWithYield( unsigned int objectMarker, IRoot* callingProxy ) = 0;

	// Create the object from the objectMarker. No yielding is done.
	virtual IRoot* CreateObject( unsigned int objectMarker, IRoot* callingProxy ) = 0;

	// notification to the builder that object is about to be trashed.
	// If CreateObject was returning early and doing more async building, this event would
	// be a good time to stop that process.
	virtual void OnObjectDestroyed( unsigned /*objectMarker*/, IRoot* /*callingProxy*/ ) {}

	// Notification to the builder that it is now the active LOD
	virtual void OnSelected( unsigned /* objectMarker */, IRoot* /* calling proxy */ ) {}

	// Call this after CreateObject if it returned a nullptr.
	virtual void GetErrorMessage( std::string& msg ) = 0;
};

#endif
