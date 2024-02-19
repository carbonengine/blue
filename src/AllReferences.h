////////////////////////////////////////////////////////////////////////////////
//
// Creator:		Filipp Pavlov
// Created:		February 2024
// Copyright:	CCP 2024
//

#pragma once

#include <Find.h>

template <>
struct std::hash<Be::IID>
{
	size_t operator()( const Be::IID& iid ) const
	{
		return size_t( iid.GetHash() );
	}
};

template <>
struct std::hash<IRootPtr>
{
	size_t operator()( const IRootPtr& obj ) const
	{
		return std::hash<IRoot*>()( obj.p );
	}
};


BLUE_CLASS( AllReferences ) :
	public IRoot
{
public:
	EXPOSE_TO_BLUE();

	bool Update( float sec );
	BluePy GetReferences( IRoot * obj );
	std::vector<IRootPtr> FindInterface( IRoot* obj, const char* iidName );
	void SetRoot( IRoot * root );
	IRootPtr GetRoot() const;

private:
	struct Reference
	{
		IRoot* parent;
		const Be::VarEntry* attr;
		ssize_t index;
		RouteStep::StepType type;
	};

	class SeenObjectSet
	{
	public:
		SeenObjectSet();
		bool Visit( IRoot* obj );
		void Clear();

	private:
		std::vector<std::vector<IRootPtr>> m_buckets;
	};

	struct TemporaryData
	{
		void Clear();

		std::vector<IRootPtr> stack;
		SeenObjectSet seen;
	};

	struct GeneratedData
	{
		void Clear();

		std::unordered_map<IRootPtr, std::vector<Reference>> references;
		std::unordered_map<Be::IID, std::vector<IRoot*>> byType;
	};

	bool HasRoute( IRoot* from, IRoot* to, std::unordered_map<IRoot*, bool>& hasRoute ) const;

	IRootPtr m_root;
	TemporaryData m_temp;
	GeneratedData m_new;
	GeneratedData m_current;
};

TYPEDEF_BLUECLASS( AllReferences );
