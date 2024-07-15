#pragma once
#include "Engine/Core/EngineCommon.hpp"


class TileDefinition
{
//public member variables
public:
	static std::vector<TileDefinition> s_tileDefinitions;

	//tile parameters
	std::string m_name = "invalid name";
	char		m_symbol = ' ';
	bool		m_isBlocked = true;

//public member functions
public:
	//constructor
	explicit TileDefinition(XmlElement const& element);

	//static functions
	static void InitializeTileDefs();
	static TileDefinition const* GetTileDefinitionByName(std::string name);
	static TileDefinition const* GetTileDefinitionBySymbol(std::string symbol);
};
