// Copyright © 2020 CCP ehf.

#include "StdAfx.h"
#include "crypto.h"
#include "openssl/engine.h"


BLUE_DEFINE( AsymmetricCipher );

const Be::ClassInfo* AsymmetricCipher::ExposeToBlue()
{
	EXPOSURE_BEGIN( AsymmetricCipher, "An RSA asymmetric cipher for encryption and signatures" )
		MAP_INTERFACE( AsymmetricCipher )

		MAP_METHOD_AND_WRAP
		(
			"GenerateKey",
			GenerateKey,
			"Creates a key with the given bit size. Primarily exposed for having a valid key in the test suite."
		)

		MAP_METHOD_AND_WRAP
		(
			"LoadPublicKey",
			LoadPublicKey,
			"Loads a public key from bytes.\n"
			":param key: Key string.\n"
			":param password: The password string used to decrypt the key.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"LoadPrivateKey",
			LoadPrivateKey,
			"Loads a private key from bytes and a password string.\n"
			":param key: bytes representation of the key.\n"
			":param password: The password string used to decrypt the key.\n"
		)

		MAP_METHOD
		(
			"Encrypt",
			PyEncrypt,
			"Encrypts a binary payload.\n"
			":param str: bytes to encrypt.\n"
		)

		MAP_METHOD
		(
			"Decrypt",
			PyDecrypt,
			"Decrypts a binary payload. This can only be called on a cipher which has been loaded with a private key.\n"
			":param str: bytes to decrypt.\n"
		)

		MAP_METHOD
		(
			"Sign",
			PySign,
			"Signs a binary payload and returns the signature as a bytes object.\n"
			":param str: bytes to be signed.\n"
		)

		MAP_METHOD
		(
			"VerifySignature",
			PyVerifySignature,
			"Verifies if a signature matches a given binary payload.\n"
			":param str: bytes payload for which the signature should match.\n"
			":param signature: bytes string.\n"
		)

	EXPOSURE_END()
}

BLUE_DEFINE( SymmetricCipher );

const Be::ClassInfo* SymmetricCipher::ExposeToBlue()
{
	EXPOSURE_BEGIN( SymmetricCipher, "An AES symmetric cipher for encryption" )
		MAP_INTERFACE( SymmetricCipher )

		MAP_METHOD
		(
			"LoadKey",
			PyLoadKey,
			"Loads a key/IV bytes pair.\n"
			":param key: Key.\n"
			":param iv: Initialization vector. Should be generated through blue.crypto.GenerateRandomBytes().\n"
		)

		MAP_METHOD
		(
			"Encrypt",
			PyEncrypt,
			"Encrypts a payload.\n"
			":param str: Bytes to encrypt.\n"
		)

		MAP_METHOD
		(
			"Decrypt",
			PyDecrypt,
			"Decrypts a payload.\n"
			":param str: Bytes to decrypt.\n"
		)

	EXPOSURE_END()
}


#if BLUE_WITH_PYTHON

PyObject* PyGenerateRandomBytes( PyObject* self, PyObject* args )
{
	int nBytes;
	if( !PyArg_ParseTuple( args, "i:CryptGenerateRandom", &nBytes ) )
	{
		return nullptr;
	}

	PyObject* r = PyBytes_FromStringAndSize( nullptr, nBytes );
	if( !r )
	{
		return PyErr_SetString( PyExc_RuntimeError, "GenerateRandomBytes: Object creation error" ), nullptr;
	}

	if( RAND_bytes( reinterpret_cast<unsigned char*>( PyBytes_AS_STRING( r ) ), nBytes ) != 1 )
	{
		Py_DECREF( r );
		r = nullptr;
		PyErr_SetString( PyExc_RuntimeError, "GenerateRandomBytes: Error getting rand bytes" );
	}

	return r;
}

PyObject* PyGetSharedAsymmetricCipher( PyObject* self, PyObject* args )
{
	return BlueWrapObjectForPython( GetSharedAsymmetricCipher() );
}

PyMethodDef CryptoMethods[] =
{
	{
		"GenerateRandomBytes",
		PyGenerateRandomBytes,
		METH_VARARGS,
		"Gets a string filled with cryptographically secure random bytes.\n"
		":param length: How many random bytes the string should contain.\n"
		":type length: int\n"
	},
	{
		"GetSharedAsymmetricCipher",
		PyGetSharedAsymmetricCipher,
		METH_NOARGS,
		"Gets a shared asymmetric cipher which should be initialized with the manifest key.\n"
	},
	{nullptr}
};

bool InitCryptoModule( PyObject* blueModule )
{
	static struct PyModuleDef moduleDef {
		PyModuleDef_HEAD_INIT,
		"blue.crypto",
		"",
		-1,
		CryptoMethods
	};
	auto module = PyModule_Create(&moduleDef);
	if ( ! module ) {
		CCP_LOGERR("Failed creating blue.crypto module");
		return false;
	}

	if ( PyModule_AddObject(blueModule, "crypto", module) ) {
		CCP_LOGERR("Failed adding crypto submodule to blue");
		return false;
	}

	return true;
}

#else

bool InitCryptoModule( PyObject* )
{
	return true;
}

#endif
