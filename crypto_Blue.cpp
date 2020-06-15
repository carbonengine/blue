////////////////////////////////////////////////////////////////////////////////
//
// Creator:   Kristjan Gerhardsson
// Created:   May 2020
// Copyright: CCP 2020
//

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
			"LoadPublicKey",
			LoadPublicKey,
			"Loads a public key from a string.\n"
			":param key: Key string.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"LoadPrivateKey",
			LoadPrivateKey,
			"Loads a private key from a key string and a password string.\n"
			":param key: Key string.\n"
			":param password: The password string used to decrypt the key.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"Encrypt",
			Encrypt,
			"Encrypts a string.\n"
			":param str: String to encrypt.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"Decrypt",
			Decrypt,
			"Decrypts a string. This can only be called on a cipher which has been loaded with a private key.\n"
			":param str: String to decrypt.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"Sign",
			Sign,
			"Signs a string and returns a signature string.\n"
			":param str: A string to be signed.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"VerifySignature",
			VerifySignature,
			"Verifies if a signature matches a given string.\n"
			":param str: The string for which the signature should match.\n"
			":param signature: A signature string.\n"
		)

	EXPOSURE_END()
}

BLUE_DEFINE( SymmetricCipher );

const Be::ClassInfo* SymmetricCipher::ExposeToBlue()
{
	EXPOSURE_BEGIN( SymmetricCipher, "An AES symmetric cipher for encryption" )
		MAP_INTERFACE( SymmetricCipher )

		MAP_METHOD_AND_WRAP
		(
			"LoadKey",
			LoadKey,
			"Loads a key/IV string pair.\n"
			":param key: Key string.\n"
			":param iv: Initialization vector. Should be generated through blue.crypto.GenerateRandomBytes().\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"Encrypt",
			Encrypt,
			"Encrypts a string.\n"
			":param str: String to encrypt.\n"
		)

		MAP_METHOD_AND_WRAP
		(
			"Decrypt",
			Decrypt,
			"Decrypts a string.\n"
			":param str: String to decrypt.\n"
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

	PyObject* r = PyString_FromStringAndSize( nullptr, nBytes );
	if( !r )
	{
		return PyErr_SetString( PyExc_RuntimeError, "GenerateRandomBytes: Object creation error" ), nullptr;
	}

	if( RAND_bytes( reinterpret_cast<unsigned char*>( PyString_AS_STRING( r ) ), nBytes ) != 1 )
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
	{0}
};

bool InitCryptoModule()
{
	return Py_InitModule( "blue.crypto", CryptoMethods );
}

#else

bool InitCryptoModule()
{
	return true;
}

#endif