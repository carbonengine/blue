#include "StdAfx.h"

#if BLUE_WITH_PYTHON

#include "include/IBlueOS.h"
#include "BlueObjectBuilderPython.h"

//BLUE_DEFINE_INTERFACE( IBlueObjectBuilder );
BLUE_DEFINE( BlueObjectBuilderPython );

const Be::ClassInfo* BlueObjectBuilderPython::ExposeToBlue()
{
	EXPOSURE_BEGIN( BlueObjectBuilderPython, "Proxy object builder that calls a python method to construct the object." )
		MAP_INTERFACE( BlueObjectBuilderPython )
		MAP_INTERFACE( IBlueObjectBuilder )

		MAP_METHOD( "SetCreateMethod", PySetCreateMethod, 
			"Sets the method to invoke when the builder needs to create an object."
			"\n"
			"\nArguments:"
			"\nCreateMethod - a method which has the following signature:"
			"\n           Arguments:"
			"\n           marker - the unique marker of the object that must be created"				
			"\n           proxy - pointer back to the caller; beware circular dependencies!"
			)

		MAP_METHOD( "SetDestroyHandler", PySetDestroyHandler, 
			"Sets the event handler that's invoked when the proxy is about to destroy its object."
			"\n"
			"\nArguments:"
			"\nDestroyHandler - a method which has the following signature:"
			"\n           Arguments:"
			"\n           marker - the unique marker of the object that must be created"				
			"\n           proxy - pointer back to the caller; beware circular dependencies!"
			)

		MAP_METHOD( "SetSelectedHandler", PySetSelectedHandler, 
			"Sets the event handler that's invoked when the proxy is becoming the active LOD."
			"\n"
			"\nArguments:"
			"\nDestroyHandler - a method which has the following signature:"
			"\n           Arguments:"
			"\n           marker - the unique marker of the object that must be created"
			"\n           proxy - pointer back to the caller; beware circular dependencies!"
			)

	EXPOSURE_END()
}

#endif
