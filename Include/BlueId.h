/* 
	*************************************************************************

	BlueId.h

	Author:    Matthias Gudmundsson
	Created:   Nov. 2000
	OS:        Win32
	Project:   Blue

	Description:   

		Declaration of all of Blue's class id's and interface id's 


	Dependencies:

		Blue

	(c) CCP 2000

	*************************************************************************
*/

#ifndef _BLUEID_H_
#define _BLUEID_H_


#ifndef _BLUE_H_INCLUDING
#error BlueId.h can only be included through Blue.h
#endif


// Blue interfaces

// Blue basic stuff
BLUE_DECLARE_INTERFACE_NO_PTR(IRoot);
BLUE_DECLARE_INTERFACE(IBlueOS);
BLUE_DECLARE_INTERFACE(INotify);
BLUE_DECLARE_INTERFACE(IInitialize);


// Blue aux. stuff
BLUE_DECLARE_INTERFACE(IList);
BLUE_DECLARE_INTERFACE(IListNotify);
BLUE_DECLARE_INTERFACE(IWeakObject);
BLUE_DECLARE_INTERFACE(IMotherLode);
BLUE_DECLARE_INTERFACE(ICacheable);


// Python stuff
BLUE_DECLARE_INTERFACE(IBluePyOS);
BLUE_DECLARE_INTERFACE(IPythonEvents);
BLUE_DECLARE_INTERFACE(IPythonMethods);
BLUE_DECLARE_INTERFACE(IPythonNumeric);


// Blue persist mechanism
BLUE_DECLARE_INTERFACE(IBlueStream);
BLUE_DECLARE_INTERFACE(IReader);
BLUE_DECLARE_INTERFACE(IWriter);
BLUE_DECLARE_INTERFACE(IFilerProperties);
BLUE_DECLARE_INTERFACE(ICopier);
BLUE_DECLARE_INTERFACE(IResFile);
BLUE_DECLARE_INTERFACE(IBlueMemStream);


// Blue classes
BLUE_DECLARE(List);
BLUE_DECLARE(PythonEvents);
BLUE_DECLARE(TaskletTimer);
BLUE_DECLARE_INTERFACE( IBlueObjectRecycler );

#endif
