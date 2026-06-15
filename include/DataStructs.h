// Copyright © 2002 CCP ehf.

/* 
	*************************************************************************

	DataStructs.h

	Project:   Blue

	Description:   

		Data structure definitions for Data project.


	Dependencies:

		Blue

	*************************************************************************
*/

#ifndef _DATASTRUCTS_H_
#define _DATASTRUCTS_H_

#include <vector>
#include <map>


typedef int DataKey;


struct FieldDef
{
	enum VARTYPE
	{
		TYPE_INT8,
		TYPE_INT16,
		TYPE_INT32,
		TYPE_INT = TYPE_INT32, 
		TYPE_INT64,
		TYPE_BOOL,
		TYPE_CHAR,
		TYPE_FLOAT,
		TYPE_DOUBLE,
		TYPE_TIME,
		TYPE_STRING,
		TYPE_PYOBJECT,
		TYPE_FK,
	};
	
	VARTYPE mType;

	enum FLAGS
	{
		FLAG_ALLOWNULLS,
		FLAG_LIST,
		FLAG_DICT,
	};

	int mFlags;
	char *mName;

	// This only used for TYPE_FK
	struct StructDefLink* mRefStruct;
};



struct DataField
{
	union
	{
		int mInteger;
		bool mBool;
		char mChar;
		float mFloat;
		double mDouble;
		__int64 mTime;
		__int64 mBigInt;
		char* mString;
		PyObject* mPyObject;
		DataKey mFK;
		
		// List and dict - start by implementing
		// a list of references or a dict of key=id, value=reference
		// The first first impl is just to make a dict which is simply
		// a list of fk integer id's
		struct {
			DataKey* mForeignKeys;
			int mListSize;
		};
	};
};



struct StructDef
{
	struct DbStore* mDbStore;
	char* mPythonClass;
	struct StructDefLink* mBase;
	int mDataSize;
	char* mPkName;

	std::vector<FieldDef> mFieldDefs;


	void AddField(FieldDef::VARTYPE vt, const char* name, int flag = 0);
	void AddRefField(const char* name, const char* refstruct, int flag = 0);
	unsigned GetSize() const;
	struct Table* GetTable();

};


// Flat data storage with index on PK
struct Table
{
	StructDef* mDefinition;

	typedef std::map<DataKey, struct DataRec*> Entries;
	Entries mEntries;
};


struct StructDefLink
{
	char* mName;
	StructDef* mStructDef;
	Table* mTable;
};



#pragma warning(disable: 4200)

struct DataRec
{
	StructDef* mDefinition;
	int mId;
	DataField mFields[];

	void* operator new(unsigned size, StructDef* def, int id = 0);
	void operator delete(void* _this);
};

#pragma warning(default: 4200)



struct DbStore
{

	// Struct definition storage
	typedef std::vector<StructDefLink*> StructDefLinks;
	StructDefLinks mStructDefLinks;

	StructDef* NewStructDef(
		const char* name, 
		const char* base, 
		const char* pythonclass,
		const char* primaryKeyField
		);

	StructDefLink* GetStructDefLink(const char* name);


	// Flat table storage
	typedef std::vector<Table> Tables;
	Tables mTables;

	Table* GetTable(const char* name);
};


#endif


