//header for the crypto.cpp file
#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#ifdef _WIN32

#include <wincrypt.h>
#include <string>
#include <vector>

extern HCRYPTPROV verCtxt;
extern HCRYPTKEY verKey;
extern HCRYPTKEY verCryptKey;
extern const char codeTimeStamp[];

//automatic KeyHandle class
class CryptKey
{
public:
	CryptKey() : hKey(0) {}
	~CryptKey() {Destroy();}
	operator HCRYPTKEY () const {return hKey;}
	HCRYPTKEY * operator &() {return &hKey;}
	void Destroy() {if (hKey) CryptDestroyKey(hKey);  hKey = 0; }
	HCRYPTKEY hKey;
};

//utomatic hash handle
class CryptHash
{
public:
	CryptHash() : hHash(0) {}
	~CryptHash() {
		if (hHash)
			CryptDestroyHash(hHash);
	}
	operator HCRYPTHASH () const {return hHash;}
	HCRYPTHASH * operator &() {return &hHash;}
	HCRYPTHASH hHash;
};

typedef std::vector<std::wstring> directives_t;

bool InitCrypto(void);
bool InitVerificationCtxt();
bool VerifyManifestFile(int &failType, std::wstring &errmsg, bool pyerr, directives_t &directives, const wchar_t *fname);

//special extra function to provide a workaround for cryptoapi deficiancy for win2000 and lower
BOOL WINAPI CryptGenPrivateExponentOneKey(
		HCRYPTPROV hProv, ALG_ID Algid, DWORD flags, HCRYPTKEY *phKey);

#endif // _WIN32

#endif