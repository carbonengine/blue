// Copyright © 2014 CCP ehf.

#include "StdAfx.h"
#include "crypto.h"

#include "openssl/engine.h"

#include <sstream>


#define SSL_RETURN_ON_ERROR( result ) \
{ \
	if( ( result ) != 1 ) \
	{ \
		return GetLastSSLError(); \
	} \
}

#define RSA_RETURN_ON_ERROR( result ) \
{ \
	if( !( result ) ) \
	{ \
		return GetLastSSLError(); \
	} \
}

#define BIO_RETURN_ON_ERROR( result, expectedValue ) \
{ \
	if( ( result ) != ( expectedValue ) ) \
	{ \
		return GetLastSSLError(); \
	} \
}


std::string GetLastSSLError()
{
	return ERR_error_string( ERR_get_error(), nullptr );
}

bool InitCrypto()
{
	return ERR_load_crypto_strings() && OpenSSL_add_all_algorithms();
}

// Get shared asymmetric cipher which is initialized with the manifest key
AsymmetricCipher* GetSharedAsymmetricCipher()
{
	static AsymmetricCipherPtr cipher;
	if( !cipher )
	{
		cipher.CreateInstance();
	}
	return cipher;
}

Be::Result<std::string> SHA256( const void* buffer, size_t length, std::string& returnValue )
{
	SHA256_CTX sha256;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	SSL_RETURN_ON_ERROR( SHA256_Init( &sha256 ) );
	SSL_RETURN_ON_ERROR( SHA256_Update( &sha256, buffer, length ) );
	SSL_RETURN_ON_ERROR( SHA256_Final( hash, &sha256 ) );

	returnValue = std::string( std::begin( hash ), std::end( hash ) );
	return std::string();
}

Be::Result<std::string> SHA256( const std::string& value, std::string& returnValue )
{
	return SHA256( value.c_str(), value.length(), returnValue );
}


SymmetricCipher::SymmetricCipher( IRoot* lockobj ) {}

SymmetricCipher::~SymmetricCipher() {}

Be::Result<std::string> SymmetricCipher::IsValid() const
{
	return m_encryptCtx ? std::string() : "Invalid context";
}

PyObject *SymmetricCipher::PyLoadKey( PyObject *self, PyObject *args )
{
	const char *key;
	const char *iv;
	Py_ssize_t keyLen, ivLen;

	if ( !PyArg_ParseTuple( args, "y#y#", &key, &keyLen, &iv, &ivLen ) )
	{
		return nullptr;
	}

	if( keyLen != 32 )
	{
		PyErr_SetString(PyExc_ValueError, "Key must be 32 bytes long");
		return nullptr;
	}
	else if( ivLen != 16 )
	{
		PyErr_SetString(PyExc_ValueError, "IV must be 16 bytes long");
		return nullptr;
	}

	auto *_this = BluePythonCast<SymmetricCipher*>( self );
	_this->LoadKey( std::string( key, keyLen ), std::string( iv, ivLen ) );

	Py_RETURN_NONE;
}

void SymmetricCipher::LoadKey( const std::string& key, const std::string& iv )
{
	m_encryptCtx = EVP_CIPHER_CTX_ptr( EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free );
	m_decryptCtx = EVP_CIPHER_CTX_ptr( EVP_CIPHER_CTX_new(), ::EVP_CIPHER_CTX_free );

	m_key = key;
	m_iv = iv;
}

PyObject *SymmetricCipher::PyEncrypt( PyObject* self, PyObject* args )
{
	const char *bytes;
	Py_ssize_t len;

	if ( !PyArg_ParseTuple( args, "y#", &bytes, &len ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<SymmetricCipher*>( self );
	std::string payload( bytes, len );
	std::string returnValue;
	auto result = _this->Encrypt( payload, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	return PyBytes_FromStringAndSize( returnValue.c_str(), returnValue.size() );
}

Be::Result<std::string> SymmetricCipher::Encrypt( const std::string& plainText, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	SSL_RETURN_ON_ERROR( EVP_EncryptInit_ex( m_encryptCtx.get(), EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>( m_key.c_str() ), reinterpret_cast<const unsigned char*>( m_iv.c_str() ) ) );

	std::vector<unsigned char> buffer( plainText.length() + EVP_CIPHER_CTX_block_size( m_encryptCtx.get() ) * 2 );
	int length, totalLength;
	SSL_RETURN_ON_ERROR( EVP_EncryptUpdate( m_encryptCtx.get(), &buffer[0], &length, reinterpret_cast<const unsigned char*>( plainText.c_str() ), static_cast<int>( plainText.length() ) ) );
	totalLength = length;

	SSL_RETURN_ON_ERROR( EVP_EncryptFinal_ex( m_encryptCtx.get(), &buffer[length], &length ) );
	totalLength += length;

	EVP_CIPHER_CTX_cleanup( m_encryptCtx.get() );

	returnValue = std::string( std::begin( buffer ), std::begin( buffer ) + totalLength );
	return std::string();
}

PyObject *SymmetricCipher::PyDecrypt( PyObject* self, PyObject* args )
{
	const char *bytes;
	Py_ssize_t len;

	if ( !PyArg_ParseTuple( args, "y#", &bytes, &len ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<SymmetricCipher*>( self );
	std::string payload( bytes, len );
	std::string returnValue;
	auto result = _this->Decrypt( payload, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	return PyBytes_FromStringAndSize( returnValue.c_str(), returnValue.size() );
}

Be::Result<std::string> SymmetricCipher::Decrypt( const std::string& encryptedText, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	SSL_RETURN_ON_ERROR( EVP_DecryptInit_ex( m_decryptCtx.get(), EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>( m_key.c_str() ), reinterpret_cast<const unsigned char*>( m_iv.c_str() ) ) );

	std::vector<unsigned char> buffer( encryptedText.length() + EVP_CIPHER_CTX_block_size( m_decryptCtx.get() ) * 2 );
	int length, totalLength;
	SSL_RETURN_ON_ERROR( EVP_DecryptUpdate( m_decryptCtx.get(), &buffer[0], &length, reinterpret_cast<const unsigned char*>( encryptedText.c_str() ), static_cast<int>( encryptedText.length() ) ) );
	totalLength = length;

	SSL_RETURN_ON_ERROR( EVP_DecryptFinal_ex( m_decryptCtx.get(), &buffer[length], &length ) );
	totalLength += length;

	EVP_CIPHER_CTX_cleanup( m_decryptCtx.get() );

	returnValue = std::string( std::begin( buffer ), std::begin( buffer ) + totalLength );
	return std::string();
}


AsymmetricCipher::AsymmetricCipher( IRoot* lockobj ) {}

AsymmetricCipher::~AsymmetricCipher() {}

Be::Result<std::string> AsymmetricCipher::IsValid() const
{
	return m_rsa ? std::string() : "Invalid context";
}

Be::Result<std::string> AsymmetricCipher::SetContext()
{
	if( m_encryptCtx )
	{
		return std::string();
	}

	EVP_PKEY_ptr pkey( EVP_PKEY_new(), ::EVP_PKEY_free );
	SSL_RETURN_ON_ERROR( EVP_PKEY_set1_RSA( pkey.get(), m_rsa.get() ) );

	EVP_PKEY_CTX_ptr encrypt( EVP_PKEY_CTX_new( pkey.get(), ENGINE_get_default_RSA() ), ::EVP_PKEY_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_PKEY_encrypt_init( encrypt.get() ) );

	EVP_PKEY_CTX_ptr decrypt( EVP_PKEY_CTX_new( pkey.get(), ENGINE_get_default_RSA() ), ::EVP_PKEY_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_PKEY_decrypt_init( decrypt.get() ) );

	EVP_MD_CTX_ptr sign = EVP_MD_CTX_ptr( EVP_MD_CTX_create(), ::EVP_MD_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_DigestSignInit( sign.get(), nullptr, EVP_sha256(), nullptr, pkey.get() ) );

	EVP_MD_CTX_ptr verify = EVP_MD_CTX_ptr( EVP_MD_CTX_create(), ::EVP_MD_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_DigestVerifyInit( verify.get(), nullptr, EVP_sha256(), nullptr, pkey.get() ) );

	m_key.swap( pkey );
	m_encryptCtx.swap( encrypt );
	m_decryptCtx.swap( decrypt );
	m_signCtx.swap( sign );
	m_verifyCtx.swap( verify );

	return std::string();
}

Be::Result<std::string> AsymmetricCipher::GenerateKey( int bitsize )
{
	RSA_ptr rsa( RSA_new(), ::RSA_free );
	BN_ptr bn( BN_new(), ::BN_free );
	EVP_PKEY_ptr pkey( EVP_PKEY_new(), ::EVP_PKEY_free );

	RSA_RETURN_ON_ERROR( BN_set_word( bn.get(), RSA_F4 ) );
	RSA_RETURN_ON_ERROR( RSA_generate_key_ex( rsa.get(), bitsize, bn.get(), nullptr ) );

	m_rsa.swap( rsa );

	BE_RETURN_ON_ERROR( SetContext() );

	return std::string();
}

Be::Result<std::string> AsymmetricCipher::LoadPublicKey( const std::string& key, const std::string& password, bool& returnValue )
{
	returnValue = false;

	BIO_ptr bo( BIO_new( BIO_s_mem() ), ::BIO_free );
	BIO_RETURN_ON_ERROR( BIO_write( bo.get(), key.c_str(), static_cast<int>( key.length() ) ), key.length() );

	RSA* rsa = m_rsa.get();
	RSA_RETURN_ON_ERROR( PEM_read_bio_RSA_PUBKEY( bo.get(), &rsa, nullptr, const_cast<char*>( password.c_str() ) ) );

	if( !m_rsa )
	{
		m_rsa = RSA_ptr( rsa, ::RSA_free );
	}

	BE_RETURN_ON_ERROR( SetContext() );

	returnValue = true;
	return std::string();
}

Be::Result<std::string> AsymmetricCipher::LoadPrivateKey( const std::string& key, const std::string& password, bool& returnValue )
{
	returnValue = false;

	BIO_ptr bo( BIO_new( BIO_s_mem() ), ::BIO_free );
	BIO_RETURN_ON_ERROR( BIO_write( bo.get(), key.c_str(), static_cast<int>( key.length() ) ), key.length() );

	RSA* rsa = m_rsa.get();
	RSA_RETURN_ON_ERROR( PEM_read_bio_RSAPrivateKey( bo.get(), &rsa, nullptr, const_cast<char*>( password.c_str() ) ) );

	if( !m_rsa )
	{
		m_rsa = RSA_ptr( rsa, ::RSA_free );
	}

	BE_RETURN_ON_ERROR( SetContext() );

	returnValue = true;
	return std::string();
}

Be::Result<std::string> AsymmetricCipher::GetPublicKey( std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	BIO_ptr bo( BIO_new( BIO_s_mem() ), ::BIO_free );
	SSL_RETURN_ON_ERROR( PEM_write_bio_RSAPublicKey( bo.get(), m_rsa.get() ) );

	std::stringstream str;
	while( true )
	{
		char buffer[1024];
		int read = BIO_read( bo.get(), buffer, sizeof( buffer ) );

		if( read < 0 )
		{
			return GetLastSSLError();
		}

		str << std::string( buffer, read );

		if( read != sizeof( buffer ) )
		{
			returnValue = str.str();
			return std::string();
		}
	}
}

Be::Result<std::string> AsymmetricCipher::GetPrivateKey( const std::string& password, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	BIO_ptr bo( BIO_new( BIO_s_mem() ), ::BIO_free );
	SSL_RETURN_ON_ERROR( PEM_write_bio_RSAPrivateKey( bo.get(), m_rsa.get(), EVP_aes_256_cbc(), nullptr, 0, nullptr, const_cast<char*>( password.c_str() ) ) );

	std::stringstream str;
	while( true )
	{
		char buffer[1024];
		int read = BIO_read( bo.get(), buffer, sizeof( buffer ) );

		if( read < 0 )
		{
			return GetLastSSLError();
		}

		str << std::string( buffer, read );

		if( read != sizeof( buffer ) )
		{
			returnValue = str.str();
			return std::string();
		}
	}
}

Be::Result<std::string> AsymmetricCipher::Encrypt( const std::string& plainText, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	size_t outlen;
	SSL_RETURN_ON_ERROR( EVP_PKEY_encrypt( m_encryptCtx.get(), nullptr, &outlen, reinterpret_cast<const unsigned char*>( plainText.c_str() ), plainText.length() ) );

	std::vector<unsigned char> buffer( outlen );
	SSL_RETURN_ON_ERROR( EVP_PKEY_encrypt( m_encryptCtx.get(), &buffer[0], &outlen, reinterpret_cast<const unsigned char*>( plainText.c_str() ), plainText.length() ) );

	returnValue = std::string( std::begin( buffer ), std::begin( buffer ) + outlen );
	return std::string();
}

Be::Result<std::string> AsymmetricCipher::Decrypt( const std::string& encryptedText, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	size_t outlen;
	SSL_RETURN_ON_ERROR( EVP_PKEY_decrypt( m_decryptCtx.get(), nullptr, &outlen, reinterpret_cast<const unsigned char*>( encryptedText.c_str() ), encryptedText.length() ) );

	std::vector<unsigned char> buffer( outlen );
	SSL_RETURN_ON_ERROR( EVP_PKEY_decrypt( m_decryptCtx.get(), &buffer[0], &outlen, reinterpret_cast<const unsigned char*>( encryptedText.c_str() ), encryptedText.length() ) );

	returnValue = std::string( std::begin( buffer ), std::begin( buffer ) + outlen );
	return std::string();
}

Be::Result<std::string> AsymmetricCipher::Sign( const std::string& text, std::string& returnValue ) const
{
	BE_RETURN_ON_ERROR( IsValid() );

	EVP_MD_CTX_ptr sign( EVP_MD_CTX_create(), ::EVP_MD_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_DigestSignInit( sign.get(), nullptr, EVP_sha256(), nullptr, m_key.get() ) );

	size_t outlen;
	SSL_RETURN_ON_ERROR( EVP_DigestSign( sign.get(), nullptr, &outlen, reinterpret_cast<const unsigned char*>( text.c_str() ), text.length() ) );

	std::vector<unsigned char> buffer( outlen );
	SSL_RETURN_ON_ERROR( EVP_DigestSign( sign.get(), &buffer[0], &outlen, reinterpret_cast<const unsigned char*>( text.c_str() ), text.length() ) );

	returnValue = std::string( std::begin( buffer ), std::begin( buffer ) + outlen );
	return std::string();
}

Be::Result<std::string> AsymmetricCipher::VerifySignature( const std::string& text, const std::string& signature, bool& returnValue ) const
{
	returnValue = false;

	BE_RETURN_ON_ERROR( IsValid() );

	EVP_MD_CTX_ptr verify( EVP_MD_CTX_create(), ::EVP_MD_CTX_free );
	SSL_RETURN_ON_ERROR( EVP_DigestVerifyInit( verify.get(), nullptr, EVP_sha256(), nullptr, m_key.get() ) );
	int result = EVP_DigestVerify( verify.get(), reinterpret_cast<const unsigned char*>( signature.c_str() ), signature.length(), reinterpret_cast<const unsigned char*>( text.c_str() ), text.length() );
	
	if( result < 0 )
	{
		return GetLastSSLError();
	}

	returnValue = result;
	return std::string();
}

PyObject* AsymmetricCipher::PyEncrypt( PyObject* self, PyObject* args )
{
	const char *bytes;
	Py_ssize_t len;

	if ( !PyArg_ParseTuple( args, "y#", &bytes, &len ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<AsymmetricCipher*>( self );
	std::string payload( bytes, len );
	std::string returnValue;
	auto result = _this->Encrypt( payload, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	return PyBytes_FromStringAndSize( returnValue.c_str(), returnValue.size() );
}

PyObject* AsymmetricCipher::PyDecrypt( PyObject* self, PyObject* args )
{
	const char *bytes;
	Py_ssize_t len;

	if ( !PyArg_ParseTuple( args, "y#", &bytes, &len ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<AsymmetricCipher*>( self );
	std::string payload( bytes, len );
	std::string returnValue;
	auto result = _this->Decrypt( payload, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	return PyBytes_FromStringAndSize( returnValue.c_str(), returnValue.size() );
}

PyObject* AsymmetricCipher::PySign( PyObject* self, PyObject* args )
{
	const char *bytes;
	Py_ssize_t len;

	if ( !PyArg_ParseTuple( args, "y#", &bytes, &len ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<AsymmetricCipher*>( self );
	std::string payload( bytes, len );
	std::string returnValue;
	auto result = _this->Sign( payload, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	return PyBytes_FromStringAndSize( returnValue.c_str(), returnValue.size() );
}

PyObject* AsymmetricCipher::PyVerifySignature( PyObject* self, PyObject* args )
{
	const char *bytes;
	const char *signature;
	Py_ssize_t lenPayload, lenSignature;

	if ( !PyArg_ParseTuple( args, "y#y#", &bytes, &lenPayload, &signature, &lenSignature ) ) {
		return nullptr;
	}

	auto *_this = BluePythonCast<AsymmetricCipher*>( self );
	std::string payload( bytes, lenPayload );
	std::string secret( signature, lenSignature );
	bool returnValue;
	auto result = _this->VerifySignature( payload, secret, returnValue );
	if ( !BeIsSuccess( result ) ) {
		PyErr_SetString( PyExc_RuntimeError, result.value.c_str() );
		return nullptr;
	}

	if ( returnValue ) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}
