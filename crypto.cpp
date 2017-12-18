//Crypto
//Add stuff for CryptoApi
#include "StdAfx.h"

#if BLUE_WITH_PYTHON
#ifdef _WIN32

#include "crypto.h"
#include "win32.h"
#include "manifest.h"
#include <new.h>
#include "PyTemplates.h"

#include <wincrypt.h>

#include <vector>
#include <string>

#include "Include/IBlueOS.h"
#include "Include/IBluePersist.h"

#include "resource.h"
#include "zlib.h"


//the actual blue datat
#include "bluekey.h"

//A Class to softload stuff.
class SoftLoader {
public:
	void Init() {}

	SoftLoader() {
#define LOAD(L, N) m##N = ( N##_t ) Load(L, #N)
	LOAD("crypt32.dll", CryptBinaryToStringA);
	LOAD("crypt32.dll", CryptStringToBinaryA);
#undef LOAD
}

	static void *Load(const char *dll, const char *function)
	{
		HMODULE h = GetModuleHandle(dll); //we assume the module is already loaded
		if (!h) return 0;
		return GetProcAddress(h, function);
	}

//typedefs
typedef BOOL (WINAPI *CryptBinaryToStringA_t)(const BYTE*, DWORD, DWORD, LPSTR, DWORD*);
typedef BOOL (WINAPI *CryptStringToBinaryA_t)(LPCSTR, DWORD, DWORD, BYTE*, DWORD*, DWORD*, DWORD*);
#define DEF(N) \
	N##_t m##N;\
	N##_t N() {Init(); return m##N;}

DEF(CryptBinaryToStringA)
DEF(CryptStringToBinaryA)
#undef DEF
};
static SoftLoader *loader = 0;


// handles for our verification context
HCRYPTPROV verCtxt = 0;
HCRYPTKEY  verKey = 0;
HCRYPTKEY  verCryptKey = 0;


//Python function prototypes:
#define PROTO(N) PyObject *Py##N(PyObject *self, PyObject *args);
//som early experimental functions.
PROTO(GenSignatureKeyPair)
PROTO(SignData)
PROTO(VerifySignature)

//manifest stuff
PROTO(PackManifest)
PROTO(VerifyManifestFile)

//access to verification context
PROTO(GetVerContext)
PROTO(GetVerKey)
PROTO(GetVerCryptKey)

//special X509 blob conversion
PROTO(PublicKeyBlobToX509Blob)

//CryptAPI raw stuff
PROTO(CryptAcquireContext)
PROTO(CryptGenKey)
PROTO(CryptGenPrivateExponentOneKey)
PROTO(CryptGetKeyParam)
PROTO(CryptSetKeyParam)
PROTO(CryptExportKey)
PROTO(CryptImportKey)
PROTO(CryptDeriveKey)
PROTO(CryptGetUserKey)
PROTO(CryptCreateHash)
PROTO(CryptDuplicateHash)
PROTO(CryptGetHashParam)
PROTO(CryptHashData)
PROTO(CryptHashSessionKey)
PROTO(CryptSignHash)
PROTO(CryptVerifySignature)
PROTO(CryptEncrypt)
PROTO(CryptDecrypt)
PROTO(CryptGenRandom)
PROTO(CryptStringToBinary)
PROTO(CryptBinaryToString)

//C version of the stuff in nasty, because it is used by zipimport before nasty has been added
PROTO(UnjumbleString)

//Crc32 stuff, used for sanity checks
PROTO(Crc32)


PyMethodDef methods[] = {
#define DEF(N) {#N, Py##N, METH_VARARGS},
DEF(GenSignatureKeyPair)
DEF(SignData)
DEF(VerifySignature)

DEF(PackManifest)
DEF(VerifyManifestFile)

DEF(GetVerContext)
DEF(GetVerKey)
DEF(GetVerCryptKey)

DEF(PublicKeyBlobToX509Blob)

DEF(CryptAcquireContext)
DEF(CryptGenKey)
DEF(CryptGenPrivateExponentOneKey) //special workaround function 
DEF(CryptGetKeyParam)
DEF(CryptSetKeyParam)
DEF(CryptExportKey)
DEF(CryptImportKey)
DEF(CryptDeriveKey)
DEF(CryptGetUserKey)
DEF(CryptCreateHash)
DEF(CryptDuplicateHash)
DEF(CryptGetHashParam)
DEF(CryptHashData)
DEF(CryptHashSessionKey)
DEF(CryptSignHash)
DEF(CryptVerifySignature)
DEF(CryptEncrypt)
DEF(CryptDecrypt)
DEF(CryptGenRandom)
DEF(CryptStringToBinary)
DEF(CryptBinaryToString)


DEF(UnjumbleString)
DEF(Crc32)

#undef DEF
{0}
};




//PythonTypes, wrapping handles
class PyCryptProv : public PyXObject2<PyCryptProv>
{
public:
	PyCryptProv() : hProv(0) {}
	~PyCryptProv() {
		if (hProv)
			CryptReleaseContext(hProv, 0);
	}
	static PyCryptProv *New() {return (PyCryptProv*)_New(PyCryptProv::GetType(), 0, 0);}
	HCRYPTPROV * operator & () {return &hProv;} //for initialization
	operator HCRYPTPROV () const {return hProv;}

	PYTHON_CLASS("blue.crypto.CryptProv");
	PYTHON_METHODS_BEGIN()
		METHOD_NOARGS(Release, "Release context")
	PYTHON_METHODS_END()
		
	static bool InitType(PyTypeObject *tp) {
		tp->tp_new = 0; //no external create
		return true;
	}
	bool Check() {
		if (!hProv)
			return PyErr_SetString(PyExc_ValueError, "CryptProv released"), false;
		return true;
	}

	PyObject *Release() {
		if (hProv) {
			if (!CryptReleaseContext(hProv, 0))
				return PyWin32Error("CryptReleaseContext");
			hProv = 0;
		}
		Py_INCREF(Py_None);
		return Py_None;
	}

public:
	HCRYPTPROV hProv;
};


class PyCryptKey : public PyXObject2<PyCryptKey>
{
public:
	PyCryptKey() : hKey(0) {}
	~PyCryptKey() {
		if (hKey)
			CryptDestroyKey(hKey);
	}
	HCRYPTKEY * operator & () {return &hKey;} //for initialization
	operator HCRYPTKEY () const {return hKey;}
	static PyCryptKey *New() {return (PyCryptKey*)_New(PyCryptKey::GetType(), 0, 0);}

	PYTHON_CLASS("blue.crypto.CryptKey");
	PYTHON_METHODS_BEGIN()
		METHOD_NOARGS(Destroy, "Release handle")
	PYTHON_METHODS_END()
		
	static bool InitType(PyTypeObject *tp) {
		tp->tp_new = 0; //no external create
		return true;
	}

	bool Check() {
		if (!hKey)
			return PyErr_SetString(PyExc_ValueError, "CryptKey destroyed"), false;
		return true;
	}

	PyObject *Destroy() {
		if (hKey) {
			if (!CryptDestroyKey(hKey))
				return PyWin32Error("CryptDestroyKey");
			hKey = 0;
		}
		Py_INCREF(Py_None);
		return Py_None;
	}

public:
	HCRYPTKEY hKey;
};


class PyCryptHash : public PyXObject2<PyCryptHash>
{
public:
	PyCryptHash() : hHash(0) {}
	~PyCryptHash() {
		if (hHash)
			CryptDestroyHash(hHash);
	}
	static PyCryptHash *New() {return (PyCryptHash*)_New(PyCryptHash::GetType(), 0, 0);}
	HCRYPTHASH * operator & () {return &hHash;} //for initialization
	operator HCRYPTHASH () const {return hHash;}

	PYTHON_CLASS("blue.crypto.CryptHash");
	PYTHON_METHODS_BEGIN()
		METHOD_NOARGS(Destroy, "Destroy hash")
	PYTHON_METHODS_END()
		
	static bool InitType(PyTypeObject *tp) {
		tp->tp_new = 0; //no external create
		return true;
	}

	bool Check() {
		if (!hHash)
			return PyErr_SetString(PyExc_ValueError, "CryptHash destroyed"), false;
		return true;
	}


	PyObject *Destroy() {
		if (hHash) {
			if (!CryptDestroyHash(hHash))
				return PyWin32Error("CryptDestroyHash");
			hHash = 0;
		}
		Py_INCREF(Py_None);
		return Py_None;
	}

public:
	HCRYPTHASH hHash;
};


	


//this class maintains a temporary key container using the "resource allocation is initialization"
//idiom
class TmpContainer
{
public:
	TmpContainer() :hProv(0) {
		if (!CryptAcquireContext(&hProv, 0, MS_ENHANCED_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
			PyWin32Error("CryptAcquireContext");
	}
	~TmpContainer() {
		if (hProv) {
			CryptReleaseContext(hProv, 0);
		}
	}
	
	operator HCRYPTPROV () const {return hProv;}
	HCRYPTPROV hProv;
};




//wholesale experimental routimes
PyObject *PyGenSignatureKeyPair(PyObject *self, PyObject *args)
{
	if(!PyArg_ParseTuple(args, ":GetSignatureKeyPair"))
		return 0;

	TmpContainer tmp;
	if (!tmp)
		return 0;
	
	//create a signature keypair
	CryptKey hKey;
	if (!CryptGenKey(tmp.hProv, AT_SIGNATURE, (1024<<16) | CRYPT_ARCHIVABLE, &hKey)) {
		PyWin32Error("CryptGenKey");
	}

	//export it (publicprivate first)
	DWORD blobLen;
	if (!CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, 0, &blobLen))
		return PyWin32Error("CryptExportKey");
	
	BluePy prstr(PyString_FromStringAndSize(0, blobLen));
	if (!prstr)
		return 0;
	
	if (!CryptExportKey(hKey, 0, PRIVATEKEYBLOB, 0, (BYTE*)PyString_AS_STRING(prstr.o), &blobLen))
		return PyWin32Error("CryptExportKey");
	_PyString_Resize(&prstr.o, blobLen);
	if (!prstr)
		return 0;
	
	//now the public key
	if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, 0, &blobLen))
		return PyWin32Error("CryptExportKey");
	
	BluePy pustr(PyString_FromStringAndSize(0, blobLen));
	if (!pustr)
		return 0;
	if (!CryptExportKey(hKey, 0, PUBLICKEYBLOB, 0, (BYTE*)PyString_AS_STRING(pustr.o), &blobLen))
		return PyWin32Error("CryptExportKey");
	_PyString_Resize(&pustr.o, blobLen);
	if (!pustr)
		return 0;


	return Py_BuildValue("OO", pustr.o, prstr.o);
}


PyObject *PySignData(PyObject *self, PyObject *args)
{
	const BYTE *data, *prkey;
	int datalen, prkeylen;
	if(!PyArg_ParseTuple(args, "s#s#:SignData", &data, &datalen, &prkey, &prkeylen))
		return 0;

	TmpContainer hProv;
	if (!hProv)
		return 0;

	//create MD5 Hash object
	CryptHash hHash;
	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
		PyWin32Error("CryptCreateHash");
		
	//import key
	CryptKey hKey;
	if (!CryptImportKey(hProv, prkey, prkeylen, 0, 0, &hKey))
		return PyWin32Error("CryptImportKey");
		
	//Hash the data
	if (!CryptHashData(hHash, data, datalen, 0))
		return PyWin32Error("CryptHashData");
		
	//sign the hash
	DWORD sigLen;
	if (!CryptSignHash(hHash, AT_SIGNATURE, 0, 0, 0, &sigLen))
		return PyWin32Error("CryptSignHash");
		
	BluePy r(PyString_FromStringAndSize(0, sigLen));
	if (!r)
		return 0;

	if (!CryptSignHash(hHash, AT_SIGNATURE, 0, 0, (BYTE*)PyString_AS_STRING(r.o), &sigLen))
		return PyWin32Error("CryptSignHash");
	_PyString_Resize(&r.o, sigLen);
		
	return r.Detach();
}


PyObject *PyVerifySignature(PyObject *self, PyObject *args)
{
	const BYTE *data, *signature, *pukey;
	int datalen, signaturelen, pukeylen;
	if(!PyArg_ParseTuple(args, "s#s#s#:VerifyDataHash", &data, &datalen, &signature, &signaturelen, &pukey, &pukeylen))
		return 0;

	TmpContainer hProv;
	if (!hProv)
		return 0;

	//create MD5 Hash object
	CryptHash hHash;
	if (!CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
		PyWin32Error("CryptCreateHash");
		
	//import key
	CryptKey hKey;
	if (!CryptImportKey(hProv, pukey, pukeylen, 0, 0, &hKey))
		PyWin32Error("CryptImportKey");
		
	//Hash the data
	if (!CryptHashData(hHash, data, datalen, 0))
		PyWin32Error("CryptHashData");
		
	//verify signature
	BOOL ok = CryptVerifySignature(hHash, signature, signaturelen, hKey, 0, 0);
	if (!ok && GetLastError() != NTE_BAD_SIGNATURE)
		return PyWin32Error("CryptVerifySignature");

	PyObject *r = ok?Py_True:Py_False;
	Py_INCREF(r);
	return r;
}


///////////////////////////
// Manifest stuff
//

//Initialize verification context.  Keys are imported from blobs and stored in static handles.
bool InitVerificationCtxt()
{
	//first, make sure we are win2000 sp2 or greater
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;

	// Initialize the OSVERSIONINFOEX structure.
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = 5;
	osvi.dwMinorVersion = 0;
	osvi.wServicePackMajor = 2;

	// Initialize the condition mask.

	VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );

	BOOL verOK = VerifyVersionInfo(&osvi, 
      VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
      dwlConditionMask);
	if (!verOK) {
		DWORD err = GetLastError();
		BeOS->SetError(BE32, 0, "VerifyVersionInfo");
		if(err == ERROR_OLD_WIN_VERSION) {
			const wchar_t *msg = L"Windows 2000 SP 2 or higher required";
			wchar_t msg_buf[1024];
			HMODULE blue;
			blue = GetModuleHandle("blue");
			if (!blue)
				blue = GetModuleHandle("blueD");
			if (blue) {
				if (LoadStringW(blue, IDS_INVALIDWINDOWS, msg_buf, sizeof(msg_buf)/sizeof(*msg_buf)))
					msg = msg_buf;
			}
			MessageBoxW(0, msg, 0, MB_ICONERROR | MB_OK);
		}
		return false;
	}

	//initialize a static verification context
	if (!CryptAcquireContext(&verCtxt, 0, MS_ENHANCED_PROV, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
	{
		BeOS->SetError(BE32, 0, "CryptAcquireContext");
		return false;
	}
	if (!CryptImportKey(verCtxt, (BYTE*)codeSigKey, sizeof(codeSigKey), 0, 0, &verKey))
	{
		BeOS->SetError(BE32, 0, "CryptImportKey");
		return false;
	}
	//now use hoops to import the crypt key.  PLAINTEXTKEYBLOB not available on older platforms
	CryptKey pKey;
	if (!CryptGenPrivateExponentOneKey(verCtxt, AT_KEYEXCHANGE, 0, &pKey))
	{
		BeOS->SetError(BE32, 0, "CryptGenPrivateExponentOneKey");
		return false;
	}
	if (!CryptImportKey(verCtxt, (BYTE*)codeCryptKey, sizeof(codeCryptKey), pKey, 0, &verCryptKey))
	{
		BeOS->SetError(BE32, 0, "CryptImportKey");
		return false;
	}

	return true;
}


PyObject *PyPackManifest(PyObject *self, PyObject *args)
{
	PyObject *seq;
	const char *timeStamp;
	PyCryptProv *hProv;
	ALG_ID algid;
	if (!PyArg_ParseTuple(args, "OsO!i:PackManifest", &seq, &timeStamp, PyCryptProv::GetType(), &hProv, &algid))
		return 0;
	if (!hProv->Check())
		return 0;
	
	Py_ssize_t nItems = PySequence_Size(seq);
	if (nItems<0)
		return 0;

	return PyPackManifest_Impl(seq, timeStamp, *hProv, algid);
}


PyObject *PyVerifyManifestFile(PyObject *self, PyObject *args)
{
	PyObject *fn;
	if (!PyArg_ParseTuple(args, "O:VerifyManifestFile", &fn))
		return 0;
	PyObject *fnu = PyUnicode_FromObject(fn);
	if (!fnu)
		return 0;
	std::wstring errmsg;
	directives_t directives;
	int failType;
	bool ok = VerifyManifestFile(failType, errmsg, true, directives, PyUnicode_AS_UNICODE(fnu));
	Py_DECREF(fnu);
	if (!ok) return 0;
	//create the return list of directives
	PyObject *r = PyList_New(directives.size());
	for(size_t i = 0; i<directives.size(); i++) {
		PyObject *s = PyUnicode_FromUnicode(directives[i].c_str(), directives[i].size());
		if (!s) {
			Py_DECREF(r);
			return 0;
		}
		PyList_SET_ITEM(r, i, s);
	}
	return r;
}


//The following functions get the static handles to the stuff in the verification context
PyObject *PyGetVerContext(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetVerContext"))
		return 0;
	static PyCryptProv *ctxt;
	if (!ctxt) {
		ctxt = PyCryptProv::New();
		if (!ctxt)
			return 0;
	}
	if (!ctxt->hProv)
		ctxt->hProv = verCtxt;
	if (!ctxt->hProv)
		return PyErr_SetString(PyExc_RuntimeError, "no context"), 0;
	Py_INCREF(ctxt);
	return ctxt;
}
PyObject *PyGetVerKey(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetVerKey"))
		return 0;
	static PyCryptKey *key;
	if (!key) {
		key = PyCryptKey::New();
		if (!key)
			return 0;
	}
	if (!key->hKey)
		key->hKey = verKey;
	if (!key->hKey)
		return PyErr_SetString(PyExc_RuntimeError, "no key"), 0;
	Py_INCREF(key);
	return key;
}
PyObject *PyGetVerCryptKey(PyObject *self, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ":GetVerCryptKey"))
		return 0;
	static PyCryptKey *key;
	if (!key) {
		key = PyCryptKey::New();
		if (!key)
			return 0;
	}
	if (!key->hKey)
		key->hKey = verCryptKey;
	if (!key->hKey)
		return PyErr_SetString(PyExc_RuntimeError, "no key"), 0;
	Py_INCREF(key);
	return key;
}



/////////////////////////////////////////////
// Raw cryptapi stuff

PyObject *PyCryptAcquireContext(PyObject *self, PyObject *args)
{
	char *container, *provider;
	int provType, flags=0;
	if (!PyArg_ParseTuple(args, "zzi|i:CryptAquireContext", &container, &provider, &provType, &flags))
		return 0;

	PyCryptProv* hProv = 0;
	if (!(flags & CRYPT_DELETEKEYSET)) {
		hProv = PyCryptProv::New();
		if (!hProv)
			return 0;
	}

	HCRYPTPROV tmp;
	if (!CryptAcquireContext(&tmp, container, provider, provType, flags)) {
		Py_XDECREF(hProv);
		return PyWin32Error("CryptAcquireContext");
	}
	if (hProv) {
		hProv->hProv = tmp;
		return hProv;
	}
	Py_INCREF(Py_None);
	return Py_None;
}


// Key management
PyObject *PyCryptGenKey(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	int algid, flags = 0;
	if (!PyArg_ParseTuple(args, "O!i|i:CryptGenKey", PyCryptProv::GetType(), &hProv, &algid, &flags))
		return 0;
	if (!hProv->Check()) return 0;
	PyCryptKey* hKey = PyCryptKey::New();
	if (!hKey)
		return 0;

	if (!CryptGenKey(*hProv, algid, flags, &(*hKey))) {
		Py_DECREF(hKey);
		return PyWin32Error("CryptGenKey");
	}
	return hKey;
}


PyObject *PyCryptGenPrivateExponentOneKey(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	int algid, flags = 0;
	if (!PyArg_ParseTuple(args, "O!i|i:CryptGenPrivateExponentOneKey",
		                  PyCryptProv::GetType(), &hProv, &algid, &flags))
		return 0;
	if (!hProv->Check()) return 0;
	PyCryptKey* hKey = PyCryptKey::New();
	if (!hKey)
		return 0;

	if (!CryptGenPrivateExponentOneKey(*hProv, algid, flags, &(*hKey))) {
		Py_DECREF(hKey);
		return PyWin32Error("CryptGenPrivateExponentOneKey");
	}
	return hKey;
}


PyObject *PyCryptGetKeyParam(PyObject *self, PyObject *args)
{
	PyCryptKey *hKey;
	int param, flags = 0;
	if (!PyArg_ParseTuple(args, "O!i|i:CryptGetKeyParam", PyCryptKey::GetType(), &hKey, &param, &flags))
		return 0;
	if (!hKey->Check()) return false;

	DWORD dataLen = 0;
	if (!CryptGetKeyParam(*hKey, param, 0, &dataLen, flags))
		return PyWin32Error("CryptGetKeyParam");
	std::vector<BYTE> data(dataLen);
	if (!CryptGetKeyParam(*hKey, param, &data[0], &dataLen, flags))
		return PyWin32Error("CryptGetKeyParam");
	data.resize(dataLen);
	

	switch(param) {
	case KP_ALGID: {
		ALG_ID id = *(ALG_ID*)&data[0];
		return PyInt_FromLong(id); }
	case KP_BLOCKLEN:
	case KP_KEYLEN:
	case KP_PERMISSIONS:
	case KP_EFFECTIVE_KEYLEN:
	case KP_PADDING:
	case KP_MODE:
	case KP_MODE_BITS: {
		if (dataLen != sizeof(DWORD))
			return PyString_FromStringAndSize((char*)&data[0], dataLen);
		DWORD dw = *(DWORD*)&data[0];
		return PyInt_FromLong(dw); }
	default:
		return PyString_FromStringAndSize((char*)&data[0], dataLen);
	}
}


PyObject *PyCryptSetKeyParam(PyObject *self, PyObject *args)
{
	PyCryptKey *hKey;
	PyObject *val;
	int param, flags = 0;
	if (!PyArg_ParseTuple(args, "O!iO|i:CryptSetKeyParam", PyCryptKey::GetType(), &hKey, &param, &val, &flags))
		return 0;
	if (!hKey->Check()) return false;

	//get the data.  Either it is an int or a string
	Py_ssize_t len;
	DWORD dataLen;
	BYTE *data;
	DWORD dw;
	if (PyString_AsStringAndSize(val, (char**)&data, &len)) {
		dw = PyInt_AsLong(val);
		if (dw == -1 && PyErr_Occurred())
			return 0;
		data = (BYTE*)&dw;
		dataLen = sizeof(dw);
	} else
		dataLen = (DWORD)len;

	//handle salt
	CRYPT_DATA_BLOB saltData;
	if (param == KP_SALT)
		param = KP_SALT_EX;
	if (param == KP_SALT_EX) {
		saltData.pbData = data;
		saltData.cbData = dataLen;
		data = (BYTE*) &saltData;
	}

	//hm, we need length checking here
	if (!CryptSetKeyParam(*hKey, param, data, flags))
		return PyWin32Error("CryptSetKeyParam");

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyCryptExportKey(PyObject *self, PyObject *args)
{
	PyCryptKey *hKey, *hExpKey;
	int blobType, flags = 0;
	if (!PyArg_ParseTuple(args, "O!Oi|i:CryptExportKey", PyCryptKey::GetType(), &hKey, &hExpKey, &blobType, &flags))
		return 0;
	if (!hKey->Check()) return false;
	if (hExpKey == Py_None)
		hExpKey = 0;
	else if (!PyObject_IsInstance(hExpKey, (PyObject*)PyCryptKey::GetType()))
		return PyErr_SetString(PyExc_TypeError, "CryptKey object or None expected"), 0;
	else if (!hExpKey->Check())
		return 0;

	DWORD dataLen;
	if (!CryptExportKey(*hKey, hExpKey?*hExpKey:0, blobType, flags, 0, &dataLen))
		return PyWin32Error("CryptExportKey");
	PyObject *r = PyString_FromStringAndSize(0, dataLen);
	if (!r)
		return 0;

	if (!CryptExportKey(*hKey, hExpKey?*hExpKey:0, blobType, flags, (BYTE*)PyString_AS_STRING(r), &dataLen)) {
		Py_DECREF(r);
		return PyWin32Error("CryptExportKey");
	}
	_PyString_Resize(&r, dataLen);
	return r;
}


PyObject *PyCryptImportKey(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	const BYTE *data;
	int datalen;
	PyCryptKey *pubKey;
	int flags = 0;
	if (!PyArg_ParseTuple(args, "O!s#O|i:CryptImportKey", PyCryptProv::GetType(), &hProv, &data, &datalen, &pubKey, &flags))
		return 0;
	if (!hProv->Check()) return 0;
	if (pubKey == Py_None)
		pubKey = 0;
	else if (!PyObject_IsInstance(pubKey, (PyObject*)PyCryptKey::GetType()))
		return PyErr_SetString(PyExc_TypeError, "CryptKey object or None expected"), 0;
	else if (!pubKey->Check())
		return 0;

	PyCryptKey *hKey = PyCryptKey::New();
	if (!hKey) return 0;

	if (!CryptImportKey(*hProv, data, datalen, pubKey?*pubKey:0, flags, &(*hKey)))
		return PyWin32Error("CryptImportKey");

	return hKey;
}


PyObject *PyCryptDeriveKey(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	ALG_ID algid;
	PyCryptHash *hHash;
	DWORD flags = 0;
	if (!PyArg_ParseTuple(args, "O!iO!|i:CryptDeriveKey", PyCryptProv::GetType(), &hProv, &algid, PyCryptHash::GetType(), &hHash, &flags))
		return 0;
	if (!hProv->Check() || !hHash->Check())
		return 0;

	PyCryptKey *hKey = PyCryptKey::New();
	if (!hKey) return 0;

	if (!CryptDeriveKey(*hProv, algid, *hHash, flags, &(*hKey))) {
		Py_DECREF(hKey);
		return PyWin32Error("CryptDeriveKey");
	}
	return hKey;
}


PyObject *PyCryptGetUserKey(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	DWORD keySpec;
	if (!PyArg_ParseTuple(args, "O!i:CryptGetUserKey", PyCryptProv::GetType(), &hProv, &keySpec))
		return 0;
	if (!hProv->Check())
		return 0;

	PyCryptKey *hKey = PyCryptKey::New();
	if (!hKey) return 0;

	if (!CryptGetUserKey(*hProv, keySpec, &(*hKey))) {
		Py_DECREF(hKey);
		return PyWin32Error("CryptGetUserKey");
	}

	return hKey;
}


//Hashing
PyObject *PyCryptCreateHash(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	int algid;
	PyCryptKey *hKey;
	int flags = 0;
	if (!PyArg_ParseTuple(args, "O!iO|i:CryptCreateHash", PyCryptProv::GetType(), &hProv, &algid, &hKey, &flags))
		return 0;
	if (!hProv->Check())
		return 0;
	if (hKey == Py_None)
		hKey = 0;
	else if (!PyObject_IsInstance(hKey, (PyObject*)PyCryptKey::GetType()))
		return PyErr_SetString(PyExc_TypeError, "CryptKey object or None expected"), 0;
	else if (!hKey->Check())
		return 0;

	PyCryptHash *hHash = PyCryptHash::New();
	if (!hHash) return 0;

	if (!CryptCreateHash(*hProv, algid, hKey?*hKey:0, flags, &(*hHash))) {
		Py_DECREF(hHash);
		return PyWin32Error("CryptCreateHash");
	}
	return hHash;
}


PyObject *PyCryptDuplicateHash(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	if (!PyArg_ParseTuple(args, "O!:CryptDuplicateHash", PyCryptHash::GetType(), &hHash))
		return 0;
	if (!hHash->Check())
		return 0;

	PyCryptHash *nHash = PyCryptHash::New();
	if (!nHash) return 0;
	
	if (!CryptDuplicateHash(*hHash, 0, 0, &(*nHash)))
		return PyWin32Error("CryptDuplicateHash");
	return nHash;
}


PyObject *PyCryptGetHashParam(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	int param, flags=0;
	if (!PyArg_ParseTuple(args, "O!i|i:CryptGetHashParam", PyCryptHash::GetType(), &hHash, &param, &flags))
		return 0;
	if (!hHash->Check())
		return 0;

	DWORD datalen;
	if (!CryptGetHashParam(*hHash, param, 0, &datalen, flags))
		return PyWin32Error("CryptGetHashParam");
	BYTE *buf = new BYTE[datalen];
	if (!buf)
		return PyErr_NoMemory();
	if (!CryptGetHashParam(*hHash, param, buf, &datalen, flags)) {
		delete [] buf;
		return PyWin32Error("CryptGetHashParam");
	}
	PyObject *r;
	if (param == HP_ALGID || param == HP_HASHSIZE) {
		_ASSERT(datalen >= sizeof(DWORD));
		r = PyInt_FromLong(*(DWORD*)buf);
	} else
		r = PyString_FromStringAndSize((const char*)buf, datalen);
	delete [] buf;
	return r;
}


PyObject *PyCryptHashData(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	const BYTE *data;
	int datalen, flags=0;
	if (!PyArg_ParseTuple(args, "O!s#|i:CryptHashData", PyCryptHash::GetType(), &hHash, &data, &datalen, &flags))
		return 0;
	if (!hHash->Check())
		return 0;

	if (!CryptHashData(*hHash, data, datalen, flags))
		return PyWin32Error("CryptHashData");

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyCryptHashSessionKey(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	PyCryptKey *hKey;
	int flags=0;
	if (!PyArg_ParseTuple(args, "O!O!|i:CryptHashSessionKey", PyCryptHash::GetType(), &hHash, PyCryptKey::GetType(), &hKey, &flags))
		return 0;
	if (!hHash->Check() || !hKey->Check())
		return 0;

	if (!CryptHashSessionKey(*hHash, *hKey, flags))
		return PyWin32Error("CryptHashSessionKey");
	Py_INCREF(Py_None);
	return Py_None;
}


PyObject *PyCryptSignHash(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	int keyspec, flags=0;
	if (!PyArg_ParseTuple(args, "O!i|i:CryptSignHash", PyCryptHash::GetType(), &hHash, &keyspec, &flags))
		return 0;
	if (!hHash->Check())
		return 0;

	DWORD datalen;
	if (!CryptSignHash(*hHash, keyspec, 0, flags, 0, &datalen))
		return PyWin32Error("CryptSignHash");
	PyObject *r = PyString_FromStringAndSize(0, datalen);
	if (!r) return 0;
	if (!CryptSignHash(*hHash, keyspec, 0, flags, (BYTE*)PyString_AS_STRING(r), &datalen)) {
		Py_DECREF(r);
		return PyWin32Error("CryptSignHash");
	}
	if (_PyString_Resize(&r, datalen)) return 0;
	return r;
}


PyObject *PyCryptVerifySignature(PyObject *self, PyObject *args)
{
	PyCryptHash *hHash;
	const BYTE *sig;
	DWORD siglen;
	PyCryptKey *hKey;
	DWORD flags=0;
	if (!PyArg_ParseTuple(args, "O!s#O!|i:CryptVerifySignature", PyCryptHash::GetType(), &hHash, &sig, &siglen, PyCryptKey::GetType(), &hKey, &flags))
		return 0;
	if (!hHash->Check() || !hKey->Check())
		return 0;

	BOOL ok = CryptVerifySignature(*hHash, sig, siglen, *hKey, 0, flags);
	if (!ok && GetLastError() != NTE_BAD_SIGNATURE)
		return PyWin32Error("CryptVerifySignature");

	PyObject *r = ok?Py_True:Py_False;
	Py_INCREF(r);
	return r;
}


//Encryption, decryption

PyObject *PyCryptEncrypt(PyObject *self, PyObject *args)
{
	PyCryptKey *hKey;
	PyCryptHash *hHash;
	PyObject *finalO;
	DWORD flags;
	const BYTE *indata;
	DWORD indatalen;
	if (!PyArg_ParseTuple(args, "O!OOis#:CryptEncrypt", PyCryptKey::GetType(), &hKey, &hHash, &finalO, &flags, &indata, &indatalen))
		return 0;
	if (!hKey->Check())
		return 0;
	BOOL final = PyObject_IsTrue(finalO);

	if (hHash == Py_None)
		hHash = 0;
	else if (!PyObject_IsInstance(hHash, (PyObject*)PyCryptHash::GetType()))
		return PyErr_SetString(PyExc_TypeError, "CryptHash object or None expected"), 0;
	else if (!hHash->Check())
		return 0;

	DWORD bufSize = indatalen;
	if (!CryptEncrypt(*hKey, hHash?*hHash:0, final, flags, 0, &bufSize, 0))
		return PyWin32Error("CryptEncrypt");

	PyObject *r = PyString_FromStringAndSize(0, bufSize);
	if (!r) return 0;

	_ASSERT(bufSize >= indatalen);
	memcpy(PyString_AS_STRING(r), indata, indatalen);

	if (!CryptEncrypt(*hKey, hHash?*hHash:0, final, flags, (BYTE*)PyString_AS_STRING(r), &indatalen, bufSize)) {
		Py_DECREF(r);
		return PyWin32Error("CryptEncrypt");
	}

	return r;
}

PyObject *PyCryptDecrypt(PyObject *self, PyObject *args)
{
	PyCryptKey *hKey;
	PyCryptHash *hHash;
	PyObject *finalO;
	DWORD flags;
	const BYTE *indata;
	DWORD indatalen;
	if (!PyArg_ParseTuple(args, "O!OOis#:CryptDecrypt", PyCryptKey::GetType(), &hKey, &hHash, &finalO, &flags, &indata, &indatalen))
		return 0;
	if (!hKey->Check())
		return 0;

	BOOL final = PyObject_IsTrue(finalO);

	if (hHash == Py_None)
		hHash = 0;
	else if (!PyObject_IsInstance(hHash, (PyObject*)PyCryptHash::GetType()))
		return PyErr_SetString(PyExc_TypeError, "CryptHash object or None expected"), 0;
	else if (!hHash->Check())
		return 0;

	PyObject *r = PyString_FromStringAndSize(0, indatalen);
	if (!r)
		return 0;
	memcpy(PyString_AS_STRING(r), indata, indatalen);
	if (!CryptDecrypt(*hKey, hHash?*hHash:0, final, flags, (BYTE*)PyString_AS_STRING(r), &indatalen)) {
		Py_DECREF(r);
		return PyWin32Error("CryptDecrypt");
	}
	if (_PyString_Resize(&r, indatalen))
		return 0;
	return r;
}


PyObject *PyCryptGenRandom(PyObject *self, PyObject *args)
{
	PyCryptProv *hProv;
	char *iv = 0;
	int nBytes;
	if (!PyArg_ParseTuple(args, "O!i:CryptGenRandom", PyCryptProv::GetType(), &hProv, &nBytes)) {
		PyErr_Clear();
		//try to get a string instead
		if (!PyArg_ParseTuple(args, "O!s#:CryptGenRandom", PyCryptProv::GetType(), &hProv, &iv, &nBytes))
			return 0;
	}
	if (!hProv->Check())
		return 0;
	PyObject *r = PyString_FromStringAndSize(0, nBytes);
	if (!r) return 0;
	if (iv)
		memcpy(PyString_AS_STRING(r), iv, nBytes); //user supplied seed
	else
		memset(PyString_AS_STRING(r), 0, nBytes);
	if (!CryptGenRandom(*hProv, nBytes, (BYTE*)PyString_AS_STRING(r))) {
		Py_DECREF(r);
		return PyWin32Error("CryptGenRandom");
	}
	return r;
}


PyObject *PyCryptBinaryToString(PyObject *self, PyObject *args)
{
	const BYTE *data;
	int datalen;
	int flags;
	if (!PyArg_ParseTuple(args, "s#i:CryptBinaryToString", &data, &datalen, &flags))
		return 0;

	if (!loader->CryptBinaryToStringA())
		return PyErr_SetString(PyExc_NotImplementedError, "CryptBinaryToString"), 0;

	DWORD cchString = 0;
	if (!loader->CryptBinaryToStringA()(data, datalen, flags, 0, &cchString))
		return PyWin32Error("CryptBinaryToString");
	char *tmp = new char [cchString];
	PyObject *result = 0;
	if (loader->CryptBinaryToStringA()(data, datalen, flags, tmp, &cchString))
		result = PyString_FromString(tmp);
	else
		return PyWin32Error("CryptBinaryToString");
	delete[] tmp;
	return result;
}


PyObject *PyCryptStringToBinary(PyObject *self, PyObject *args)
{
	const char *str;
	int flags;
	if (!PyArg_ParseTuple(args, "si:CryptStringToBinary", &str, &flags))
		return 0;

	if (!loader->CryptStringToBinaryA())
		return PyErr_SetString(PyExc_NotImplementedError, "CryptStringToBinary"), 0;

	DWORD cbBinary = 0;
	DWORD dwSkip, dwFlags;
	if (!loader->CryptStringToBinaryA()(str, 0, flags, 0, &cbBinary, &dwSkip, &dwFlags))
		return PyWin32Error("CryptStringToBinary");

	PyObject *result = PyString_FromStringAndSize(0, cbBinary);
	if (!result)
		return 0;
	if (!loader->CryptStringToBinaryA()(str, 0, flags, (BYTE*)PyString_AS_STRING(result), &cbBinary, &dwSkip, &dwFlags)) {
		Py_DECREF(result);
		return PyWin32Error("CryptStringToBinary");
	}
	return Py_BuildValue("Nii", result, dwSkip, dwFlags);
}


//C implementation of nasty.UnjumbleString.  Used by zipimport before it has had the chance to import nasty
PyObject *PyUnjumbleString(PyObject *self, PyObject *args)
{
	PyObject *s;
	PyObject *zip = Py_False;
	if (!PyArg_ParseTuple(args, "O|O", &s, &zip))
		return 0;
	BluePyTuple et(0);
	BluePy key(PyGetVerCryptKey(Py_None, et));
	if (!key) return 0;

	//actual decrypt
	BluePy arg(Py_BuildValue("OOOiO", key.o, Py_None, Py_True, 0, s));
	if (!arg) return 0;
	BluePy decr(PyCryptDecrypt(Py_None, arg));
	if (!decr) return 0;

	if (PyObject_IsTrue(zip)) {
		//must unzip
		BluePy zm(PyImport_ImportModule("zlib"));
		if (!zm) return 0;
		BluePy unzipped(PyObject_CallMethod(zm, "decompress", "O", decr.o));
		decr = unzipped;
	}
	return decr.Detach();
}


PyObject *PyCrc32(PyObject *self, PyObject *args)
{
	const char *str;
	int len;
	int start = 1;
	if (!PyArg_ParseTuple(args, "s#|i", &str, &len, &start))
		return 0;
	return PyInt_FromLong(crc32(start, (const unsigned char*)str, len));
}


static bool ConvertKey(BYTE *pbBlob, DWORD cbBlob, BYTE **ppOut, DWORD *cbOutLen)
{
	BYTE	*pbData;			//decoded data
	BYTE	*pbData1;			//decoded data
	DWORD	cbData;
	CERT_PUBLIC_KEY_INFO pkinfo;
	LPCSTR	subjectpublickeyinfo		=	"subjectpublickeyinfo" ;
	LPCSTR	rsapublickey		=	"rsapublickey" ;
	BOOL isRSAPublicKey	= FALSE;
	BOOL isPUBLICKEYBLOB = FALSE;
	#define ENCODING_TYPE  (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)

	//----- Sanity Check .. see if this is an RSAPublicKey or SubjectPublicKeyInfo blob
	cbData=0;

	if (CryptDecodeObject(ENCODING_TYPE, X509_PUBLIC_KEY_INFO, pbBlob, cbBlob, 0, NULL, &cbData)) {
		*ppOut = 0;
		*cbOutLen = 0;
		//File is a valid SubjectPublicKeyInfo blob
		return true;
	}

	if (CryptDecodeObject(ENCODING_TYPE,RSA_CSP_PUBLICKEYBLOB, pbBlob, cbBlob, 0, NULL, &cbData)) {
		isRSAPublicKey = TRUE;
		//not the normal case
	}

	//---  Try to Encode to RSA_PUBLICKey if blob is a PUBLICKEYBLOB (the normal case) ---------

	cbData=0;
	if (CryptEncodeObject(ENCODING_TYPE, RSA_CSP_PUBLICKEYBLOB, pbBlob, NULL, &cbData)) {
		pbData = (BYTE *)malloc(cbData);
		if (!pbData)
			return PyErr_NoMemory(), false;
		if (CryptEncodeObject(ENCODING_TYPE, RSA_CSP_PUBLICKEYBLOB, pbBlob, pbData, &cbData)) {
			//normal case
			isPUBLICKEYBLOB = TRUE;
		} else {
			free(pbData);
			PyWin32Error("CryptEncodeObject");
			return false;
		}
	} else {
		pbData = pbBlob;   //maybe file blob is already a RSAPublicKey
		cbData = cbBlob;
	}

	if(!isRSAPublicKey && !isPUBLICKEYBLOB) {
		if (pbData && pbData != pbBlob)
			free(pbData);
		PyErr_SetString(PyExc_ValueError, "Blob is not a recognized keyblob format") ;
		return false;
	}
	//----- We have a valid RSAPublicKey; try to encode to SubjectPublicKeyInfo  --------------

	// First build the CERT_PUBLIC_KEY_INFO struct ---------

	pkinfo.Algorithm.pszObjId = szOID_RSA_RSA;
	pkinfo.Algorithm.Parameters.cbData = 2;
	pkinfo.Algorithm.Parameters.pbData = (BYTE *) "\005";
	pkinfo.PublicKey.cbData = cbData;
	pkinfo.PublicKey.pbData = pbData;
	pkinfo.PublicKey.cUnusedBits = 0;


	cbData=0;
	if (!CryptEncodeObject(ENCODING_TYPE, X509_PUBLIC_KEY_INFO, &pkinfo, 0, &cbData))	{
		if (pbData && pbData != pbBlob)
			free(pbData);
		PyWin32Error("CryptEncodeObject");
		return false;
	}
	pbData1 = (BYTE *)malloc(cbData);
	if (!pbData1)
			return PyErr_NoMemory(), false;
	BOOL ok = CryptEncodeObject(ENCODING_TYPE, X509_PUBLIC_KEY_INFO, &pkinfo, pbData1, &cbData);
	if (pbData && pbData != pbBlob)
		free(pbData);
	
	if (ok) {
		*ppOut = pbData1;
		*cbOutLen = cbData;
		return true;
	}
	free(pbData);
	PyWin32Error("CryptEncodeObject");
	return false;
}


PyObject *PyPublicKeyBlobToX509Blob(PyObject *self, PyObject *args)
{
	PyObject *arg;
	if (!PyArg_ParseTuple(args, "O:PublicKeyBlobToX509Blob", &arg))
		return 0;

	BYTE *data;
	int datalen;
	if (!PyArg_ParseTuple(args, "s#:PublicKeyBlobToX509Blob", &data, &datalen))
		return 0;

	BYTE *res=0;
	DWORD reslen;
	if (!ConvertKey(data, datalen, &res, &reslen))
		return 0;

	if (!res) {
		//key was correct format already
		Py_INCREF(arg);
		return arg;
	}

	PyObject *result = PyString_FromStringAndSize((const char*)res, reslen);
	free(res);
	return result;
}


//initialize the module, man
bool InitCrypto(void)
{
	loader = CCP_NEW( "InitCrypto/loader" ) SoftLoader;
	
	PyObject *m = Py_InitModule("blue.crypto", methods);
	if (!m) return false;

	if (PyModule_AddObject(m, "CryptProv", (PyObject*)PyCryptProv::GetType())) return false;
	if (PyModule_AddObject(m, "CryptKey", (PyObject*)PyCryptKey::GetType())) return false;
	if (PyModule_AddObject(m, "CryptHash", (PyObject*)PyCryptHash::GetType())) return false;

	//add string constants
#define S(c) if (PyModule_AddStringConstant(m, #c, c)) return false
#define I(c) if (PyModule_AddIntConstant(m, #c, c)) return false
#define II(c) if (PyModule_AddIntConstant(m, #c, (int)(c))) return false
	S(MS_DEF_PROV);
	S(MS_ENHANCED_PROV);
	S(MS_STRONG_PROV);
	S(MS_DEF_RSA_SIG_PROV);
	S(MS_DEF_RSA_SCHANNEL_PROV);
	S(MS_DEF_DSS_PROV);
	S(MS_DEF_DSS_DH_PROV);
	S(MS_ENH_DSS_DH_PROV);
	S(MS_DEF_DH_SCHANNEL_PROV);
	S(MS_SCARD_PROV);

	//and integer constants
	I(PROV_RSA_FULL);
	I(PROV_RSA_AES);
	I(PROV_RSA_SIG);
	I(PROV_RSA_SCHANNEL);
	I(PROV_DSS);
	I(PROV_DSS_DH);
	I(PROV_DH_SCHANNEL);
	I(PROV_FORTEZZA);
	I(PROV_FORTEZZA);
	I(PROV_SSL);

	I(CRYPT_VERIFYCONTEXT);
	I(CRYPT_NEWKEYSET);
	I(CRYPT_MACHINE_KEYSET);
	I(CRYPT_DELETEKEYSET);
	I(CRYPT_SILENT);

	I(CALG_HMAC);
	I(CALG_MD2);
	I(CALG_MD4);
	I(CALG_MD5);
	I(CALG_SHA);
	I(CALG_SHA1);
	I(CALG_MAC);
	I(CALG_SSL3_SHAMD5);
	I(CALG_RSA_SIGN);
	I(CALG_DSS_SIGN);
	I(CALG_RSA_KEYX);
	I(CALG_DES);
	I(CALG_3DES_112);
	I(CALG_3DES);
	I(CALG_RC2);
	I(CALG_RC4);
	I(CALG_SEAL);
	I(CALG_DH_SF);
	I(CALG_DH_EPHEM);
	I(CALG_AGREEDKEY_ANY);
	I(CALG_KEA_KEYX);
	I(CALG_SKIPJACK);
	I(CALG_TEK);
	I(CALG_CYLINK_MEK);

	I(OPAQUEKEYBLOB);
	I(PRIVATEKEYBLOB);
	I(PUBLICKEYBLOB);
	I(SIMPLEBLOB);
	I(PLAINTEXTKEYBLOB);
	I(SYMMETRICWRAPKEYBLOB);

	I(CRYPT_DESTROYKEY);
	I(CRYPT_SSL2_FALLBACK);
	I(CRYPT_OAEP);
	
	I(CRYPT_EXPORTABLE);
	I(CRYPT_CREATE_SALT);
	I(CRYPT_NO_SALT);
	I(CRYPT_USER_PROTECTED);
	I(CRYPT_UPDATE_KEY);
	
	I(HP_ALGID);
	I(HP_HASHSIZE);
	I(HP_HASHVAL);
	I(HP_HMAC_INFO);

	I(CRYPT_USERDATA);
	I(CRYPT_LITTLE_ENDIAN);

	I(CRYPT_NOHASHOID);
	I(CRYPT_X931_FORMAT);

	I(AT_KEYEXCHANGE);
	I(AT_SIGNATURE);

	I(KP_ALGID);
	I(KP_BLOCKLEN);
	I(KP_KEYLEN);
	I(KP_SALT);
	I(KP_SALT_EX);
	I(KP_PERMISSIONS);
	I(KP_P);
	I(KP_Q);
	I(KP_G);
	I(KP_X);
	I(KP_EFFECTIVE_KEYLEN);
	I(KP_IV);
	I(KP_PADDING);
	I(KP_MODE);
	I(KP_MODE_BITS);
	I(KP_PUB_PARAMS);

	//block modes
	I(CRYPT_MODE_CBC);
	I(CRYPT_MODE_CFB);
	I(CRYPT_MODE_ECB);
	I(CRYPT_MODE_OFB);

	//key permission flag bits
	I(CRYPT_ARCHIVE);
	I(CRYPT_DECRYPT);
	I(CRYPT_ENCRYPT);
	I(CRYPT_EXPORT);
	I(CRYPT_MAC);
	I(CRYPT_READ);
	I(CRYPT_WRITE);

	//CryptBinaryToString stuff
	I(CRYPT_STRING_BASE64HEADER);
	I(CRYPT_STRING_BASE64);
	I(CRYPT_STRING_BINARY);
	I(CRYPT_STRING_BASE64REQUESTHEADER);
	I(CRYPT_STRING_HEX);
	I(CRYPT_STRING_HEXASCII);
	I(CRYPT_STRING_BASE64X509CRLHEADER);
	I(CRYPT_STRING_HEXADDR);
	I(CRYPT_STRING_HEXASCIIADDR);
	I(CRYPT_STRING_BASE64_ANY);
	I(CRYPT_STRING_ANY);
	I(CRYPT_STRING_HEX_ANY);


	return true;
}



//Special exponent one generation function.  See http://support.microsoft.com/default.aspx?scid=kb;EN-US;q228786
BOOL WINAPI CryptGenPrivateExponentOneKey(HCRYPTPROV hProv, ALG_ID Algid, DWORD flags, HCRYPTKEY *phKey)
{
	if ((Algid != AT_KEYEXCHANGE) && (Algid != AT_SIGNATURE)) {
		SetLastError(NTE_BAD_ALGID);
		return FALSE;
	}

	// Generate the private key
	 
	CryptKey hPrivateKey;
	if (!CryptGenKey(hProv, Algid, flags | CRYPT_EXPORTABLE, &hPrivateKey))
	{
		BeOS->SetError(BE32, 0, "CryptGenKey");
		return FALSE;
	}
		
	// Export the private key, we'll convert it to a private
	// exponent of one key
	DWORD dwkeyblob;
	if (!CryptExportKey(hPrivateKey, 0, PRIVATEKEYBLOB, 0, NULL, &dwkeyblob))
	{
		BeOS->SetError(BE32, 0, "CryptExportKey");
		return FALSE;
	}
		
	LPBYTE keyblob = (LPBYTE)LocalAlloc(LPTR, dwkeyblob);
	if (!keyblob)
		return FALSE;
	
	if (!CryptExportKey(hPrivateKey, 0, PRIVATEKEYBLOB, 0, keyblob, &dwkeyblob)) {
		LocalFree(keyblob);
		BeOS->SetError(BE32, 0, "CryptExportKey");
		return FALSE;
	}
	hPrivateKey.Destroy();
      
	// Get the bit length of the key
	DWORD dwBitLen;
	memcpy(&dwBitLen, &keyblob[12], 4);

	// Modify the Exponent in Key BLOB format
	// Key BLOB format is documented in SDK

	// Convert pubexp in rsapubkey to 1
	BYTE *ptr = &keyblob[16];
	DWORD n;
	for (n = 0; n < 4; n++)
	{
		if (n == 0) ptr[n] = 1;
		else ptr[n] = 0;
	}

	// Skip pubexp
	ptr += 4;
	// Skip modulus, prime1, prime2
	ptr += (dwBitLen/8);
	ptr += (dwBitLen/16);
	ptr += (dwBitLen/16);

	// Convert exponent1 to 1
	for (n = 0; n < (dwBitLen/16); n++)
	{
		if (n == 0) ptr[n] = 1;
		else ptr[n] = 0;
	}

	// Skip exponent1
	ptr += (dwBitLen/16);

	// Convert exponent2 to 1
	for (n = 0; n < (dwBitLen/16); n++)
	{
		if (n == 0) ptr[n] = 1;
		else ptr[n] = 0;
	}

	// Skip exponent2, coefficient
	ptr += (dwBitLen/16);
	ptr += (dwBitLen/16);

	// Convert privateExponent to 1
	for (n = 0; n < (dwBitLen/8); n++)
	{
		if (n == 0) ptr[n] = 1;
		else ptr[n] = 0;
	}

    // Import the exponent-of-one private key.
	BOOL ok = CryptImportKey(hProv, keyblob, dwkeyblob, 0, 0, phKey);
	LocalFree(keyblob);
	if (!ok)
		BeOS->SetError(BE32, 0, "CryptImportKey");
	return ok;
}

#endif
#endif
