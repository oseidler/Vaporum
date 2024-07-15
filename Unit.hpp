#pragma once
#include "Game/UnitDefinition.hpp"
#include "Engine/Math/IntVec2.hpp"


class Unit
{
//public member functions
public:
	//constructor
	Unit(UnitDefinition const* definition, IntVec2 startCoords, int ownerID);
	
	//game flow functions
	void Render() const;

//public member variables
public:
	UnitDefinition const* m_definition = nullptr;

	IntVec2 m_coords = IntVec2();
	
	int m_currentHealth = 0;

	int m_ownerID = 1;

	bool m_movedThisTurn = false;
};
