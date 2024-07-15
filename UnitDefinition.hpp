#pragma once
#include "Engine/Core/EngineCommon.hpp"


//forward declarations
class Model;


//enums
enum class UnitType
{
	TANK,
	ARTILLERY,
	INVALID
};


class UnitDefinition
{
//public member variables
public:
	static std::vector<UnitDefinition> s_unitDefinitions;

	//unit parameters
	std::string m_name = "invalid name";
	char		m_symbol = ' ';
	Model*		m_model = nullptr;
	UnitType	m_type = UnitType::INVALID;
	int			m_groundAttackDamage = 0;
	int			m_groundAttackRangeMin = 0;
	int			m_groundAttackRangeMax = 0;
	int			m_movementRange = 0;
	int			m_defense = 0;
	int			m_health = 0;

//public member functions
public:
	//constructor
	explicit UnitDefinition(XmlElement const& element);

	//static functions
	static void InitializeUnitDefs();
	static UnitDefinition const* GetUnitDefinitionByName(std::string name);
	static UnitDefinition const* GetUnitDefinitionBySymbol(std::string symbol);
};
