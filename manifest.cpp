/* 
	*************************************************************************

	Manifest.cpp

	Author:    Kristjan Valur Jonsson
	Created:   Aug. 2007
	OS:        Win32
	Project:   Blue

	Description:   

		Implementation of functions for the reading and wrinting of
		packaging manifests.

	Dependencies:

		Blue

	(c) CCP 2007

	*************************************************************************
*/

#include "StdAfx.h"

#if BLUE_WITH_PYTHON
#ifdef _WIN32

#include "manifest.h"
#include "crypto.h"
#include "win32.h"
#include "resource.h"
#include "BlueResFile.h"

#include "Include/IBlueOS.h"
#include "Include/IBluePersist.h"
#include "Include/IBluePaths.h"

#include <set>
#include <vector>
#include <string>
#include <algorithm>
#include "zlib.h"

static CcpLogChannel_t s_ch = CCP_LOG_DEFINE_CHANNEL( "Manifest" );


//String utilities
static void ToLower(std::wstring &s) {
	for (size_t i = 0; i<s.size(); i++)
		s[i] = towlower(s[i]);
}

static std::wstring Lower(const std::wstring &s) {
	std::wstring tmp(s);
	ToLower(tmp);
	return tmp;
}

//create a long (>MAX_PATH) pathname, for many API functions
static std::wstring LongName(const std::wstring &fn) {
	if (fn.size()<=MAX_PATH)
		return fn;
	if (fn.compare(0, 4, L"\\\\?\\") == 0)
		return fn;
	return L"\\\\?\\" + fn;
}

//Get the long version of path:
static std::wstring LongPathName(const std::wstring &p)
{
	std::vector<wchar_t> tmp(1);
	std::wstring lp = LongName(p);
	DWORD req = GetLongPathNameW(lp.c_str(), &tmp[0], 0);
	if (req) {
		tmp.resize(req);
		req = GetLongPathNameW(lp.c_str(), &tmp[0], req);
		if (req)
			return std::wstring(&tmp[0]);
	}
	return p;
}


// a system to store a set of filenames
typedef std::set<std::wstring> stringset;
typedef std::vector<std::wstring> stringvec;
class FileHierarchy
{
	struct Entry
	{
		Entry(const std::wstring &dir) : mDir(dir) {}
		bool operator < (const Entry &o) const {return mDir < o.mDir;}
		std::wstring mDir; //full name of dir
		stringset mFiles; //immediate subfiles
	};
public:
	void Add(const std::wstring &_file) {
		std::wstring file(_file);
		Normalize(file);
		std::wstring path, tail;
		RSplit(path, tail, file);
		GetDir(path).mFiles.insert(tail);
	}

	const stringset *Find(const std::wstring path) const {
		std::set<Entry>::const_iterator i = mDirs.find(path);
		if (i != mDirs.end())
			return &(i->mFiles);
		return 0;
	}

private:
	struct Entry &GetDir(const std::wstring &path)
	{
		std::pair<std::set<Entry>::iterator, bool> ins = mDirs.insert(path);
		if (ins.second && path.size()) {
			//just inserted it
			//Get the parent dir, insert this as an entry into it
			std::wstring lpath, tail;
			RSplit(lpath, tail, path);
			Entry &dir = GetDir(lpath);
			dir.mFiles.insert(tail);
		}
		// This is slightly evil. The insert gives us back a const iterator
		// to the Entry either found or just added. This used to be a
		// regular iterator, but with VS2010 it is a const iterator.
		// It would be cleaner to change this to a map - separate the
		// key out from the value. We know that the ordering operator
		// used on the Entry will not change so this does work - no
		// operations we can do on the entry will invalidate the set.
		// Note that the std::set documentation clearly states that
		// values should not change after they are added to the set.

		Entry& entry = const_cast<Entry&>(*ins.first);
		return entry;
	}

	static void Normalize(std::wstring &n) //turn backslashes to slashes and remove doubles.
	{
		for (size_t i=0; i<n.size(); i++)
			if (n[i] == L'\\')
				n[i] = L'/';
		size_t p = 0;
		for(;;) {
			size_t np = n.find(L"//", p);
			if (np == std::wstring::npos)
				break;
			n.erase(np, 1);
			p = np;
		}
	}

	//split a path in two at the rightmost slash, unless it ends in a slash, then the second-
	//rightmost slash.  The splitting slash is returned in the "path", the tail in the "tail"
	static void RSplit(std::wstring &path, std::wstring &tail, const std::wstring &file)
	{
		if (!file.size()) {
			path.clear();
			tail.clear();
			return;
		}
		size_t p;
		if (file[file.size()-1] == L'/')
			p = file.rfind(L'/', file.size()-2);
		else
			p = file.rfind(L'/');
		if (p != std::wstring::npos) {
			path = file.substr(0, p+1);
			tail = file.substr(p+1);
		} else {
			path.clear();
			tail=file;
		}
	}

private:
	std::set<Entry> mDirs;
};


// An entry from the manifest file
struct ManifestEntry_t
{
	ManifestEntry_t() : mCrc(0) {}
	ManifestEntry_t(const std::wstring &name, const std::vector<BYTE> &hash, __int32 crc) :
		mName(name), mHash(hash), mCrc(crc)
		{}
	std::wstring mName;
	std::vector<BYTE> mHash;
	__int32 mCrc;
};
typedef std::vector<ManifestEntry_t> Manifest_t;


//simple outbuf object, for chucking stuff into memory
class OutBuf
{
public:
	OutBuf() : mBuf(0) {
		mSize = 16; 
		mBuf = (char*)malloc(mSize);
		mPos = 0;
	}
	~OutBuf() {free(mBuf);}

	bool Ok() {
		if (!mBuf) {
			PyErr_NoMemory();
			return false;
		}
		return true;
	}

	bool CheckSpace(size_t s) {
		if (mSize - mPos >= s)
			return true;
		size_t size = mSize;
		while (size-mPos < s)
			size *= 2;
		void *tmp = realloc(mBuf, size);
		if (!tmp) {
			size = mPos+s; //try with a tighter fit
			tmp = realloc(mBuf, size);
		}
		if (!tmp) {
			PyErr_NoMemory();
			return false;
		}
		mBuf = (char*)tmp;
		mSize = size;
		return true;
	}
	bool Write(const void *data, size_t s) {
		if (CheckSpace(s)) {
			memcpy(((char*)mBuf)+mPos, data, s);
			mPos += s;
			return true;
		}
		return false;
	}
	const void *GetData() const {return mBuf;}
	size_t GetPos() const {return mPos;}
private:
	void *mBuf;
	size_t mSize;
	size_t mPos;
};

//ditto for input
class InBuf
{
public:
	InBuf(const void *buf, size_t s) : mBuf(buf), mSize(s), mPos(0) {}
	bool Read(bool pyerr, void *r, size_t s) {
		if (s <= mSize-mPos) {
			memcpy(r, ((const char*)mBuf)+mPos, s);
			mPos += s;
			return true;
		}
		if (!pyerr)
			BeOS->SetError(BEDEF, 0, "InBuf underflow");
		else
			PyErr_SetString(PyExc_RuntimeError, "InBuf underflow");
		return false;
	}
	const void *GetData() const {return mBuf;}
	size_t GetPos() const {return mPos;}
	void Reset() {mPos = 0;}
private:
	const void *mBuf;
	size_t mSize;
	size_t mPos;
};

static bool UnpackManifest(std::wstring &errmsg, bool pyerr, Manifest_t &m, std::string &timeStamp,
						   directives_t& directives,
						   const void *data, size_t datalen, HCRYPTPROV prov,
						   ALG_ID algid, HCRYPTKEY key);

static bool VerifyManifestEntry(std::wstring &errmsg, bool pyerr, __int32 &crc,
								const ManifestEntry_t &m, HCRYPTPROV prov, ALG_ID algid,
								HCRYPTKEY key);

static bool VerifyDirClosureWithRetry(std::wstring &errmsg, bool pyerr, const FileHierarchy &hierarchy,
									  const std::wstring &dir);

static bool VerifyDirClosure(std::wstring &errmsg, stringvec &xtrafiles, bool pyerr,
							 const FileHierarchy &hierarchy, const std::wstring &dir);


static bool RelocateXtraFiles(const std::wstring &dir, const stringvec &xtrafiles);

static std::wstring CanonicalName(const std::wstring &fn);

static std::wstring TranslateString(const std::wstring &def, int id);

//This function packages a manifest and key timestamp.  The manifest is a sequence of tuples, the tuples
//being filename (in unicode) and strings (of hashes).  An optional third tuple member is a crc32 value for reference.
//it then hashes and signs this, and appends the signature.  The format is then (all sizes are DWORDS)
//if the name ends in a slash and stringlength is 0, it is considered a file dir completion entry.  Then no files
//are allowed in that dir except those already mentioned.
// version 1:
// nPairs : namelen1 : name1 : signlen1 : sign1 : namelen2 : name2 : signlen2 : sign3 : ... : totalsignature (until end)
// version 2 (adds crc32 info):
// nPairs : namelen1 : name1 : signlen1 : sign1 : crc32_1: namelen2 : name2 : signlen2 : crc32_1 : ... : totalsignature (until end)
// version 3 (adds string directives)
// nEntries : type1 : (name1:signlen1:sign1:crc32_1 | directivelen1:directive)....
// each entry can be a regular manifest file (type 0) or a directive (1)

PyObject *PyPackManifest_Impl(PyObject *seq, const char *timeStamp, HCRYPTPROV hProv, ALG_ID algid)
{
	Py_ssize_t nItems = PySequence_Size(seq);
	if (nItems<0)
		return 0;

	OutBuf b;
	if (!b.Ok()) return 0;

	char version = 3;
	if (!b.Write(&version, sizeof(version))) return 0;

	DWORD size = (DWORD)nItems;
	if (!b.Write(&size, sizeof(size))) return 0;
	for(Py_ssize_t i = 0; i<nItems; i++) {
		BluePy o(PySequence_GetItem(seq, i));
		if (!o) return 0;
		char type;
		if (PyString_Check(o.o)) {
			//it is a directive
			type = 1;
			if (!b.Write(&type, sizeof(type))) return 0;
			size = (DWORD)PyString_GET_SIZE(o.o);
			if (!b.Write(&size, sizeof(size))) return 0;
			if (!b.Write(PyString_AS_STRING(o.o), PyString_GET_SIZE(o.o)*sizeof(BYTE))) return 0;
		} else {
			//regular entry
			PyObject *nameO;
			char *data;
			int datalen;
			__int32 crc = 0;
			if (!PyArg_ParseTuple(o, "Os#|i", &nameO, &data, &datalen, &crc)) return 0;
			BluePy u(PyUnicode_FromObject(nameO));
			if (!u) return 0;

			//type
			type = 0;
			if (!b.Write(&type, sizeof(type))) return 0;

			//name
			size = (DWORD)PyUnicode_GetSize(u.o);
			if (!b.Write(&size, sizeof(size))) return 0;
			if (!b.Write(PyUnicode_AS_UNICODE(u.o), size*sizeof(wchar_t))) return 0;
			//hash
			size = (DWORD)datalen;
			if (!b.Write(&size, sizeof(size))) return 0;
			if (!b.Write(data, datalen*sizeof(BYTE))) return 0;
			//crc
			if (!b.Write(&crc, sizeof(crc))) return 0;
		}
	}

	//now, add the timestamp
	size = (int)strlen(timeStamp);
	if (!b.Write(&size, sizeof(size))) return 0;
	if (!b.Write(timeStamp, size*sizeof(char))) return 0;

	// so, now we have written the manifest into the buffer.  Now to hash it:
	CryptHash hash;
	if (!CryptCreateHash(hProv, algid, 0, 0, &hash))
		return PyWin32Error("CryptCreateHash");

	if (!CryptHashData(hash, (const BYTE*)b.GetData(), (DWORD)b.GetPos(), 0)) {
		PyWin32Error("CryptHashData");
		return false;
	}

	//then, we need to sign it:
	DWORD signsize;
	if (!CryptSignHash(hash, AT_SIGNATURE, 0, 0, 0, &signsize)) {
		PyWin32Error("CryptSignHash");
		return false;
	}
	BYTE *sign = (BYTE*)malloc(signsize);
	if (!sign)
		return PyErr_NoMemory();
	if (!CryptSignHash(hash, AT_SIGNATURE, 0, 0, sign, &signsize)) {
		PyWin32Error("CryptSignHash");
		free(sign);
		return false;
	}

	//append the signature to the buffer.
	b.Write(sign, signsize);
	free(sign);

	//finally, return buffer as a string
	return PyString_FromStringAndSize((const char*)b.GetData(), b.GetPos());
}


bool VerifyManifestFile(int &type, std::wstring &errmsg, bool pyerr, directives_t &directives, const wchar_t *fname)
{
	type = 0; //unspecified failure
	IResFilePtr tmp;	
	if (!tmp.CreateInstance(GetResFileClsid())) {
		if (pyerr)
			PyOS->PyErr_BlueError();
		return false;
	}

	if (!tmp->OpenW(fname, true)) {
		if (pyerr)
			PyOS->PyErr_BlueError();
		errmsg += std::wstring(L"Couldn't open file ")+CanonicalName(fname)+L'\n';
		return false;
	}

	ssize_t fileSize = tmp->GetSize();
	if (fileSize<0) {
		if (pyerr)
			PyOS->PyErr_BlueError();
		return false;
	}

	void *buf = malloc(fileSize);
	if (!buf) {
		if (pyerr)
			return PyErr_NoMemory(), false;
		BeOS->SetError(BEDEF, 0, "No Memory");
		return false;
	}
	ssize_t read = tmp->Read(buf, fileSize);
	if (read<0) {
		free(buf);
		if (pyerr)
			PyOS->PyErr_BlueError();
		return false;
	}
	if (read != fileSize) {
		free(buf);
		if (pyerr)
			return PyErr_SetString(PyExc_RuntimeError, "Read short file"), false;
		return PyErr_SetString(PyExc_RuntimeError, "Read short file"), 0;
	}

	Manifest_t manifest;
	std::string timeStamp;
	bool ok = UnpackManifest(errmsg, pyerr, manifest, timeStamp, directives, buf, fileSize, verCtxt, CALG_SHA, verKey);
	free(buf);
	if (!ok)
		return false;

	std::wstring executablePath = BeOS->GetExecutablePath();
	bool executableChecked = false;

	FileHierarchy hierarchy;
	for (size_t i=0; i<manifest.size(); i++) {
		const std::wstring &name = manifest[i].mName;

		if( CanonicalName( name ) == executablePath )
		{
			executableChecked = true;
		}

		bool success;
		if (name.size() && name[name.size()-1] == L'/' && !manifest[i].mHash.size()) {
			//dir closure thingy
			//note, we have stopped trying to move files about.  Don't retry.
			//success = VerifyDirClosureWithRetry(errmsg, pyerr, hierarchy, name);
			stringvec xtrafiles;
			success = VerifyDirClosure(errmsg, xtrafiles, pyerr, hierarchy, name);
			if (!success)
				type = 1; //extra files
		} else {
			__int32 crc;
			success = VerifyManifestEntry(errmsg, pyerr, crc, manifest[i], verCtxt, CALG_SHA, verKey);
			hierarchy.Add(Lower(name));
			if (!success && type != 1)
				type = 2; //missing or incorrect file, but 1 trumps 2
		}
		if (!success) {
			if (pyerr)
				return false;
			ok = false;
		}
	}
	if( !executableChecked && !manifest.empty() )
	{
		ok = false;
	}

	if (ok)
		CCP_LOG_CH( s_ch, "Successfully verified %s", (char*)CW2A(fname));
	else
		CCP_LOGWARN_CH( s_ch, "Failed to verify %s", (char*)CW2A(fname));

	return ok;
}


bool UnpackManifest(std::wstring &errmsg, bool pyerr, Manifest_t &m, std::string &timeStamp,
					directives_t &directives, const void *data, size_t datalen, HCRYPTPROV prov, ALG_ID algid, HCRYPTKEY key)
{
	InBuf ib(data, datalen);
	char version;
	if (!ib.Read(pyerr, &version, sizeof(version))) return false;
	if (version != 1 && version != 2 && version != 3) {
		if (pyerr)
			return PyErr_Format(PyExc_RuntimeError, "invalid manifest version %d", version), false;
		BeOS->SetError(BEDEF, 0, "invalid manifest version %d", version);
		return false;
	}
	DWORD nPairs;
	if (!ib.Read(pyerr, &nPairs, sizeof(nPairs))) return false;

	//log progress
	CCP_LOG_CH( s_ch, "Reaing manifest file version %d, %d entries...", version, nPairs);

	std::vector<wchar_t> namebuf;
	std::vector<char> strbuf;
	std::vector<BYTE> signbuf;
	char type = 0;
	for (DWORD i = 0; i<nPairs; i++) {
		if (version >= 3)
			if (!ib.Read(pyerr, &type, sizeof(type)))
				return false; //read entry type.

		switch (type) {
		case 0: {
			DWORD size;
			if (!ib.Read(pyerr, &size, sizeof(size))) return false;
			//do sanity checking of size to disallow buffer overrun attacks.
			namebuf.resize(size+1);
			if (size < 0 || namebuf.size() <= size) {
				if (pyerr)
					return PyErr_Format(PyExc_RuntimeError, "invalid size %d words", size), false;
				BeOS->SetError(BEDEF, 0, "invalid size %d words", size);
				return false;
			}
			if (size && !ib.Read(pyerr, &(namebuf[0]), size*sizeof(wchar_t))) return false;
			namebuf[size] = 0;
			if (!ib.Read(pyerr, &size, sizeof(size))) return false;
			signbuf.resize(size);
			if (size < 0 || signbuf.size() < size) {
				if (pyerr)
					return PyErr_Format(PyExc_RuntimeError, "invalid size %d words", size), false;
				BeOS->SetError(BEDEF, 0, "invalid size %d words", size);
				return false;
			}
			if (size && !ib.Read(pyerr, &(signbuf[0]), size*sizeof(BYTE))) return false;
			__int32 crc = 0;
			if (version > 1)
				if (!ib.Read(pyerr, &crc, sizeof(crc))) return false;
			m.push_back(ManifestEntry_t(std::wstring(&(namebuf[0])), signbuf, crc));
			break;}
		case 1: {
			DWORD size;
			if (!ib.Read(pyerr, &size, sizeof(size))) return false;
			//do sanity checking of size to disallow buffer overrun attacks.
			strbuf.resize(size+1);
			if (size < 0 || strbuf.size() <= size) {
				if (pyerr)
					return PyErr_Format(PyExc_RuntimeError, "invalid size %d words", size), false;
				BeOS->SetError(BEDEF, 0, "invalid size %d words", size);
				return false;
			}
			if (size && !ib.Read(pyerr, &(strbuf[0]), size*sizeof(char))) return false;
			strbuf[size] = 0;
			//convert to unicode from utf8
			BluePy utmp(PyUnicode_DecodeUTF8(&(strbuf[0]), size, 0));
			if (!utmp) {
				if (!pyerr)
					PyOS->PyFlushError("UnpackManifest");
				return false;
			}
			directives.push_back(PyUnicode_AsUnicode(utmp));
			break; }
		default:
			if (pyerr)
				return PyErr_Format(PyExc_RuntimeError, "invalid field type %d", type), false;
			BeOS->SetError(BEDEF, 0, "invalid field type %d", type);
			return false;
		}
	}

	//now the timestamp
	DWORD ssize;
	if (!ib.Read(pyerr, &ssize, sizeof(ssize))) return false;
	std::vector<char> ts(ssize+1);
	if (!ib.Read(pyerr, &ts[0], ssize*sizeof(char))) return false;
	ts[ssize] = 0;
	timeStamp = &ts[0];

	//Log this to the logger
	CCP_LOG_CH( s_ch, "Manifest timestamp: %s, expecting %s", timeStamp.c_str(), codeTimeStamp);
	
	//now, the rest must be a signature.  Let's verify the signature by rehashing
	CryptHash hash;
	if (!CryptCreateHash(prov, algid, 0, 0, &hash)) {
		if (pyerr)
			 return PyWin32Error("CryptHashData"), false;
		BeOS->SetError(BE32, 0, "CryptHashData");
		return false;
	}

	if (!CryptHashData(hash, (const BYTE*)ib.GetData(), (DWORD)ib.GetPos(), 0)) {
		if (pyerr)
			PyWin32Error("CryptHashData");
		else
            BeOS->SetError(BE32, 0, "CryptSignData");
		return false;
	}

	if (!CryptVerifySignature(hash, (BYTE*)ib.GetData()+ib.GetPos(), (DWORD)(datalen-ib.GetPos()), key, 0, 0)) {
		if (GetLastError() == NTE_BAD_SIGNATURE) {
			wchar_t buffer[512];
			swprintf(buffer, 512, L"Bad manifest signature, timestamp \"%S\", expected \"%S\"", timeStamp.c_str(), codeTimeStamp);
			errmsg += std::wstring(buffer) + L'\n';
			CCP_LOGERR_CH( s_ch, "%", buffer);
			if (pyerr)
				PyErr_Format(PyExc_RuntimeError, "Bad manifest signature, timestamp \"%s\", expected \"%s\"", timeStamp.c_str(), codeTimeStamp);
			else
				BeOS->SetError(BEDEF, 0, "Bad manifest signature, timestamp \"%s\", expected \"%s\"", timeStamp.c_str(), codeTimeStamp);
			
		} else {
			if (pyerr)
				PyWin32Error("CryptVerifySignature");
			else
				BeOS->SetError(BE32, 0, "CryptVerifySignature");
		}
		return false;
	}
	CCP_LOG_CH( s_ch, "Manifest file verified");
	return true;
}

static bool ComputeInner(bool pyerr, __int32 &crc, CryptHash &hash, void *buf, ssize_t fileSize, const char *fname);
bool VerifyManifestEntry(std::wstring &errmsg, bool pyerr, __int32 &crc, const ManifestEntry_t &m,
						 HCRYPTPROV prov, ALG_ID algid, HCRYPTKEY key)
{
	IResFilePtr tmp;	
	if (!tmp.CreateInstance(GetResFileClsid()))
		return false;

	if (!tmp->OpenW(m.mName.c_str(), true)) {
		errmsg += TranslateString(L"File not found: ", IDS_VERIFYFAIL_NOTFOUND) + CanonicalName(m.mName) + L"\n";
		return false;
	}

	ssize_t fileSize = tmp->GetSize();
	if (fileSize<0)
		return false;

	void *buf;
	if (!tmp->LockData(&buf, 0))
		return false;
	
	//ok, file is mapped now.  let's do it.

	CryptHash hash;
	if (!CryptCreateHash(prov, algid, 0, 0, &hash)) {
		if (pyerr)
			 return PyWin32Error("CryptHashData"), false;
		BeOS->SetError(BE32, 0, "CryptHashData");
		return false;
	}

	bool ok = ComputeInner(pyerr, crc, hash, buf, fileSize, CW2A(m.mName.c_str()));
	tmp->UnlockData();
	if (!ok) {
		return false;
	}

	bool ok2 = true;
	BOOL ok3 = CryptVerifySignature(hash, (BYTE*)&m.mHash[0], (DWORD)m.mHash.size(), key, 0, 0);
	if (!ok3) {
		if (GetLastError() == NTE_BAD_SIGNATURE) {
			wchar_t buffer[512];
			std::wstring fname = m.mName;
			std::wstring text = TranslateString(L"Verification failed: \"%s\", crc:%x, expected:%x.", IDS_VERIFYFAIL_INCORRECTCRC);
			swprintf(buffer, 512, text.c_str(),
				     CanonicalName(fname).c_str(), crc, m.mCrc);
			errmsg += std::wstring(buffer) + L'\n';
			CCP_LOGERR_CH( s_ch, "%s", (char*)CW2A(buffer));

			if (pyerr)
				PyErr_Format(PyExc_RuntimeError, "bad signature for file %s, crc:%x, expected:%x",
					(char*)CW2A(m.mName.c_str()), crc, m.mCrc);
			else
				BeOS->SetError(BE32, 0, "bad signature for file %s, crc:%x, expected:%x",
					(char*)CW2A(m.mName.c_str()), crc, m.mCrc);
		} else {
			if (pyerr)
				PyWin32Error("CryptVerifySignature");
			else
				BeOS->SetError(BE32, 0, "CryptVerifySignature");
		}
		ok2 = false;
	} else
		CCP_LOG_CH( s_ch, "Verification passed: \"%s\", crc:%x", (char*)CW2A(m.mName.c_str()), crc);

	return ok2;
}

bool ComputeInner(bool pyerr, __int32 &crc, CryptHash &hash, void *buf, ssize_t fileSize, const char *fname)
{
	BOOL ok;
	__try {
		//first, compute crc, for reporting purposes
		crc = crc32(1, (const unsigned char*)buf, (unsigned int)fileSize);
		//then compute hash proper
		ok = CryptHashData(hash, (const BYTE*)buf, (DWORD)fileSize, 0);
	}
	__except(GetExceptionCode()==EXCEPTION_IN_PAGE_ERROR ?
            EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
	    // Failed to read from the view.
		if (pyerr)
			 return PyErr_SetString(PyExc_IOError, fname), false;
		BeOS->SetError(BEDEF, 0, "Failed to read from file %s", fname);
		return false;
	}
	if (!ok)
	{
		if (pyerr)
			PyWin32Error("CryptHashData");
		else
			BeOS->SetError(BE32, 0, "CryptHashData");
		return false;
	}
	return true;
}


bool VerifyDirClosureWithRetry(std::wstring &errmsg, bool pyerr, const FileHierarchy &hierarchy, const std::wstring &dir)
{
	std::wstring oldmsg = errmsg;
	stringvec xtrafiles;
	if (VerifyDirClosure(errmsg, xtrafiles, pyerr, hierarchy, dir))
		return true;

	if (!xtrafiles.size())
		return false;

	//now, we had extra files.  Try removing them.
	RelocateXtraFiles(dir, xtrafiles);
	xtrafiles.clear();
	errmsg = oldmsg;
	return VerifyDirClosure(errmsg, xtrafiles, pyerr, hierarchy, dir);
}


bool VerifyDirClosure(std::wstring &errmsg, stringvec &xtrafiles, bool pyerr, const FileHierarchy &hierarchy, const std::wstring &dir)
{
	//first, open directory and gather all files found
	stringset files;
	if (!dir.size() || dir[dir.size()-1] != L'/') {
		if (pyerr)
			PyErr_Format(PyExc_RuntimeError, "Invalid Dir Closure: %s", (char*)CW2A(dir.c_str()));
		else
			BeOS->SetError(BEDEF, 0, "Invalid Dir Closure: %s", (char*)CW2A(dir.c_str()));
		return false;
	}

	std::wstring fullpath = BePaths->ResolvePathW(dir.c_str());

	//Build the set of files in the directory
	std::wstring search(fullpath);
	search += L'*';
	WIN32_FIND_DATAW data;
	HANDLE h = FindFirstFileW(LongName(search).c_str(), &data);
	if (h==INVALID_HANDLE_VALUE) {
		if (pyerr)
			PyWin32Error("VerifyDirClosure");
		else
			BeOS->SetError(BE32, 0, "VerifyDirClosure");
		return false;
	}
	std::map<std::wstring, std::wstring> lowerToHigher;
	for(;;) {
		std::wstring fname = data.cFileName;
		if (fname != L"." && fname != L"..") {
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				fname += L'/';
			std::wstring l = Lower(fname);
			files.insert(l);
			lowerToHigher[l] = fname;
		}
		BOOL more = FindNextFileW(h, &data);
		if (!more) {
			if (GetLastError() != ERROR_NO_MORE_FILES) {
				if (pyerr)
					PyWin32Error("VerifyDirClosure");
				else
					BeOS->SetError(BE32, 0, "VerifyDirClosure");
				return false;
			}
			break;
		}
	}
	FindClose(h);

	//ignore files that we allow (currently .lbw and .cab files)
	std::vector<std::wstring> suffixes;
	suffixes.push_back(L".lbw");
	suffixes.push_back(L".cab");
	stringset ignore;
	for (stringset::iterator i = files.begin(); i!=files.end(); ++i) {
		const std::wstring &f = *i;
		for(size_t j=0; j<suffixes.size(); j++) {
			const std::wstring &suffix = suffixes[j];
			if (suffix.size()>f.size())
				continue;
			const wchar_t *tail = f.c_str()+f.size()-suffix.size();
			if (!_wcsicmp(tail, suffix.c_str())) //case insensitive compare.
				ignore.insert(f); //found a match
		}
	}
	for(stringset::iterator i = ignore.begin(); i != ignore.end(); ++i)
		files.erase(*i);

	//find the reference set from the hierarchy
	stringset emptyset;
	const stringset *hfiles = hierarchy.Find(Lower(dir));
	const stringset &refSet = hfiles ? *hfiles : emptyset;

	if (files == refSet) {
		std::wstring allfiles;
		for (stringset::const_iterator i = refSet.begin(); i!= refSet.end(); ++i) {
			allfiles += *i + L", ";
		}

		CCP_LOG_CH( s_ch, "Verified that dir %s contains only files %s",
			  (char*)CW2A(dir.c_str()), (char*)CW2A(allfiles.c_str()));
		return true;
	}

	//find the difference:  files that are in files but not in refSet.
	stringvec diff(files.size());
	stringvec::iterator end =
		set_difference(files.begin(), files.end(), refSet.begin(), refSet.end(), diff.begin());

	//reverse difference, for our interest:
	stringvec rdiff(refSet.size());
	set_difference(refSet.begin(), refSet.end(), files.begin(), files.end(), rdiff.begin());

	if (end == diff.begin()) {
		//there were no extra files, perhaps only missing files.  That is ok.
		return true;
	}

	std::wstring cdir = CanonicalName(dir);
	std::wstring msg = TranslateString(L"Unknown files found:\n", IDS_VERIFYFAIL_UNKNOWNFOUND);
	for(stringvec::iterator i = diff.begin(); i != end; ++i) {
		std::wstring &fn = lowerToHigher[*i];
		xtrafiles.push_back(fn);
		msg += CanonicalName(dir + fn) + L'\n';
	}
	errmsg += msg;

	if (pyerr)
		PyErr_Format(PyExc_RuntimeError, "VerifyDirClosure: %s", 
			(char*)CW2A(msg.c_str()));
	else
		BeOS->SetError(BEDEF, 0, "VerifyDirClosure: %s", (char*)CW2A(msg.c_str()));
	return false;
}


bool RelocateXtraFiles(const std::wstring &dir, const stringvec &xtrafiles)
{
	//construct a temporary path name
	wchar_t tmppath[MAX_PATH];
	if (!GetTempPathW(_countof(tmppath), tmppath))
		wcscpy_s(tmppath, L".");
	std::wstring tmpnam = LongPathName(tmppath);

	SYSTEMTIME stime;
	GetSystemTime(&stime);
	FILETIME ftime;
	SystemTimeToFileTime(&stime, &ftime);
	swprintf_s(tmppath, L"EVE%.4x.tmp", ftime.dwLowDateTime&0xffff);

	tmpnam = tmpnam + tmppath;
	
	// create the directory
	BOOL ok = CreateDirectoryW(LongName(tmpnam).c_str(), 0);
	if (!ok) {
		BeOS->SetError(BE32, 0, "RelocateXtraFiles: %s", (char*)CW2A(tmpnam.c_str()));
		return false;
	}

	bool success = true;
	std::wstring progress;
	for (size_t i = 0; i<xtrafiles.size(); ++i) {
		std::wstring fname = xtrafiles[i];
		bool isDir = false;
		if (fname.size() && fname[fname.size()-1] == L'/') {
			fname.resize(fname.size()-1);
			isDir = true;
		}
		std::wstring srcfile = CanonicalName(dir+fname);
		std::wstring dstfile = CanonicalName(tmpnam + L'\\' + fname);
		
		CCP_LOGWARN_CH( s_ch, "Moving %s to %s", (char*)CW2A(srcfile.c_str()), (char*)CW2A(dstfile.c_str()));
		ok = MoveFileW(LongName(srcfile).c_str(), LongName(dstfile).c_str());
		if (!ok) {
			std::wstring msg = L"Failed to move " + srcfile + L" to " + dstfile;
			CCP_LOGERR_CH( s_ch, "%s", (char*)CW2A(msg.c_str()), (char*)CW2A(dstfile.c_str()));
			progress += msg + L'\n';
			success = false;
		} else {
			progress += L"Moved " + srcfile + L" to " + dstfile + L'\n';
		}
	}
	if (progress.size()) {
		//ouput a message box for the masses
		UINT type = MB_OK;
		type |= success ? MB_ICONINFORMATION : MB_ICONEXCLAMATION;
		MessageBoxW(0, progress.c_str(),
			L"Relocating offending file(s):", 
			type);
	}
	return success;
}

// Look up a string in the string tables and translate, optionally injecting a string argument
//   def - the default value for the string, will be used if lookup is not successful
//   id - the id for the string to look up
std::wstring TranslateString(const std::wstring &def, int id)
{
	std::wstring text(def);
	wchar_t buf[256];
	
	HMODULE blue = GetModuleHandle("blue");
	if (!blue)
		blue = GetModuleHandle("blueD");
	if (blue) {
		if (LoadStringW(blue, id, buf, _countof(buf)))
			text = std::wstring(buf);
	}
	return text;
}

// make file name canonical, i.e. standard filenames
std::wstring CanonicalName(const std::wstring &_fn)
{
	std::wstring fn(_fn);

	// Get real path name
	fn = BePaths->ResolvePathW(fn.c_str());

	// flip back slashes
	for(size_t i = 0; i<fn.size(); ++i)
		if (fn[i] == L'/')
			fn[i] = L'\\';

	// remove duplicate slashes
	for(;;) {
		size_t pos = fn.find(L"\\\\");
		if (pos != std::wstring::npos)
			fn.erase(pos, 2);
		else
			break;
	}
	
	// remove all single dot parts in pathname
	for(;;) {
		size_t pos = fn.find(L"\\.\\");
		if (pos != std::wstring::npos)
			fn.erase(pos, 2);
		else
			break;
	}

	// tricky: erase all \..\ dudes
	for(;;) {
		size_t pos = fn.find(L"\\..\\");
		if (pos != std::wstring::npos) {
			size_t prior = fn.rfind(L'\\', pos-1);
			if (prior != std::wstring::npos)
				fn.erase(prior, pos-prior+3);
			else
				break;
		} else
			break;
	}
	return fn;
}
#endif
#endif
