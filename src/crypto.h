// Copyright © 2014 CCP ehf.

#pragma once

#include "openssl/evp.h"
#include "openssl/rsa.h"


#define BE_RETURN_ON_ERROR( beResult ) \
{ \
	Be::Result<std::string> _result = ( beResult ); \
	if( !BeIsSuccess( _result ) ) \
	{ \
		return _result; \
	} \
}


bool InitCryptoModule( PyObject* blueModule );
bool InitCrypto();

Be::Result<std::string> SHA256( const void* buffer, size_t length, std::string& returnValue );
Be::Result<std::string> SHA256( const std::string& value, std::string& returnValue );


BLUE_DECLARE( SymmetricCipher );

BLUE_CLASS( SymmetricCipher )
	: public IRoot
{
	using EVP_CIPHER_CTX_ptr = std::unique_ptr< EVP_CIPHER_CTX, decltype( &::EVP_CIPHER_CTX_free )>;

public:
	EXPOSE_TO_BLUE();

	SymmetricCipher( IRoot* lockobj = nullptr );
	~SymmetricCipher();

	Be::Result<std::string> IsValid() const;

	static PyObject *PyLoadKey( PyObject *self, PyObject *args );
	void LoadKey( const std::string& key, const std::string& iv );

	static PyObject *PyEncrypt( PyObject *self, PyObject *args );
	Be::Result<std::string> Encrypt( const std::string& plainText, std::string& returnValue ) const;

	static PyObject *PyDecrypt( PyObject *self, PyObject *args );
	Be::Result<std::string> Decrypt( const std::string& encryptedText, std::string& returnValue ) const;


private:
	EVP_CIPHER_CTX_ptr m_encryptCtx{ nullptr, ::EVP_CIPHER_CTX_free };
	EVP_CIPHER_CTX_ptr m_decryptCtx{ nullptr, ::EVP_CIPHER_CTX_free };

	std::string m_key;
	std::string m_iv;
};

TYPEDEF_BLUECLASS( SymmetricCipher );


BLUE_DECLARE( AsymmetricCipher );

BLUE_CLASS( AsymmetricCipher )
	: public IRoot
{
	using BN_ptr = std::unique_ptr<BIGNUM, decltype( &::BN_free )>;
	using RSA_ptr = std::unique_ptr<RSA, decltype( &::RSA_free )>;
	using EVP_PKEY_ptr = std::unique_ptr<EVP_PKEY, decltype( &::EVP_PKEY_free )>;
	using EVP_PKEY_CTX_ptr = std::unique_ptr<EVP_PKEY_CTX, decltype( &::EVP_PKEY_CTX_free )>;
	using EVP_MD_CTX_ptr = std::unique_ptr<EVP_MD_CTX, decltype( &::EVP_MD_CTX_free )>;
	using BIO_ptr = std::unique_ptr<BIO, decltype( &::BIO_free )>;

public:
	EXPOSE_TO_BLUE();

	AsymmetricCipher( IRoot* lockobj = nullptr );
	~AsymmetricCipher();

	Be::Result<std::string> IsValid() const;
	Be::Result<std::string> GenerateKey( int bitsize );
	Be::Result<std::string> LoadPublicKey( const std::string& key, const std::string& password, bool& returnValue );
	Be::Result<std::string> LoadPrivateKey( const std::string& key, const std::string& password, bool& returnValue );
	Be::Result<std::string> GetPublicKey( std::string& returnValue ) const;
	Be::Result<std::string> GetPrivateKey( const std::string& password, std::string& returnValue ) const;
	static PyObject *PyEncrypt( PyObject *self, PyObject *args );
	Be::Result<std::string> Encrypt( const std::string& plainText, std::string& returnValue ) const;
	static PyObject *PyDecrypt( PyObject *self, PyObject *args );
	Be::Result<std::string> Decrypt( const std::string& encryptedText, std::string& returnValue ) const;
	static PyObject *PySign( PyObject *self, PyObject *args );
	Be::Result<std::string> Sign( const std::string& text, std::string& returnValue ) const;
	static PyObject *PyVerifySignature( PyObject *self, PyObject *args );
	Be::Result<std::string> VerifySignature( const std::string& text, const std::string& signature, bool& returnValue ) const;

private:
	Be::Result<std::string> SetContext();

	RSA_ptr m_rsa{ nullptr, ::RSA_free };
	EVP_PKEY_ptr m_key{ nullptr, ::EVP_PKEY_free };
	EVP_PKEY_CTX_ptr m_encryptCtx{ nullptr, ::EVP_PKEY_CTX_free };
	EVP_PKEY_CTX_ptr m_decryptCtx{ nullptr, ::EVP_PKEY_CTX_free };
	EVP_MD_CTX_ptr m_signCtx{ nullptr, ::EVP_MD_CTX_free };
	EVP_MD_CTX_ptr m_verifyCtx{ nullptr, ::EVP_MD_CTX_free };
};

TYPEDEF_BLUECLASS( AsymmetricCipher );

AsymmetricCipher* GetSharedAsymmetricCipher();
