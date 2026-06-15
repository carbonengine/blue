// Copyright © 2021 CCP ehf.

#pragma once

BLUE_CLASS_IMPL( BlueClipboard )

class BlueClipboard: public IRoot
{
public:
	EXPOSE_TO_BLUE();
    
    enum OperationResult
    {
        CLIPBOARD_OK,
        CLIPBOARD_INCOMPATIBLE_FORMAT,
        CLIPBOARD_FAILURE,
    };

	OperationResult GetData( std::string& data ) const;
	OperationResult GetData( std::wstring& data ) const;
	OperationResult SetData( const std::string& data );
	OperationResult SetData( const std::wstring& data );

	static BlueClipboard& GetInstance();

#if BLUE_WITH_PYTHON
	PyObject* PyGetString() const;
	PyObject* PyGetUnicode() const;
	PyObject* PySetData( PyObject* arg );
#endif
};

TYPEDEF_BLUECLASS( BlueClipboard );
