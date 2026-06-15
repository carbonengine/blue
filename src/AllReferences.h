// Copyright © 2024 CCP ehf.

#pragma once

#include <Find.h>


BLUE_CLASS( AllReferences ) :
	public IRoot
{
public:
	~AllReferences();

	EXPOSE_TO_BLUE();

	bool Update( float sec );
	BluePy GetReferences( IRoot * obj );
	std::vector<IRootPtr> FindInterface( IRoot* obj, const char* iidName );
	void SetRoot( IRoot * root );
	IRootPtr GetRoot() const;

private:
	struct Reference
	{
		bool operator==( const Reference& other ) const;
		bool operator!=( const Reference& other ) const;

		IRoot* parent;
		union
		{
			const Be::VarEntry* attr;
			ssize_t index;
		};
		uint64_t generation;
		bool isAttribute;
	};

	struct References
	{
		Reference first = {};
		std::vector<Reference> rest;
		uint64_t generation = 0;
	};

	bool CleanReferences( uint64_t endTime );
	bool HasRoute( IRoot* from, IRoot* to, std::unordered_map<IRoot*, bool>& hasRoute ) const;
	bool IsOutdated( const Reference& ref ) const;
	bool IsOutdated( const References& refs ) const;

	IRootPtr m_root;
	std::vector<IRoot*> m_stack;
	std::unordered_map<unsigned, std::vector<IRoot*>> m_newByType;
	std::unordered_map<unsigned, std::vector<IRoot*>> m_currentByType;
	std::unordered_map<IRoot*, References> m_references;
	std::unordered_map<IRoot*, References>::iterator m_clearReferencesIt;
	uint64_t m_generation = 1;
	bool m_cleaningReferences = false;
};

TYPEDEF_BLUECLASS( AllReferences );
