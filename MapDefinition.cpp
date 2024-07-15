#include "Game/MapDefinition.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Shader.hpp"


//static variable declaration
std::vector<MapDefinition> MapDefinition::s_mapDefinitions;


//
//constructor
//
MapDefinition::MapDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);
	m_gridSize = ParseXmlAttribute(element, "gridSize", m_gridSize);
	m_boundsMin = ParseXmlAttribute(element, "worldBoundsMin", m_boundsMin);
	m_boundsMax = ParseXmlAttribute(element, "worldBoundsMax", m_boundsMax);

	std::string shaderPath = ParseXmlAttribute(element, "overlayShader", "Data/Shaders/Default");
	m_overlayShader = g_theRenderer->CreateShader(shaderPath.c_str());

	//#ToDo: Change to rely on names of elements, instead of assuming that order will be correct
	XmlElement const* tilesRootElement = element.FirstChildElement();
	GUARANTEE_OR_DIE(tilesRootElement != nullptr, "Failed to read tiles element!");

	tinyxml2::XMLNode const* tilesCDataElement = tilesRootElement->FirstChild();
	GUARANTEE_OR_DIE(tilesCDataElement != nullptr, "Failed to read tiles cdata!");
	std::string tilesCData = tilesCDataElement->Value();

	Strings tileDefRows = SplitStringOnDelimiter(tilesCData, '\n');
	for (int stringIndex = static_cast<int>(tileDefRows.size()) - 1; stringIndex > -1; stringIndex--)	//start from the bottom
	{
		Strings tileDefRow = SplitStringOnDelimiter(tileDefRows[stringIndex], ' ');
		
		for (int charIndex = 0; charIndex < tileDefRow.size(); charIndex++)
		{
			if (!tileDefRow[charIndex].empty())
			{
				std::string tileDefSymbol = tileDefRow[charIndex];
				m_tileDefs.emplace_back(tileDefSymbol);
			}
		}
	}

	XmlElement const* p1UnitsRootElement = tilesRootElement->NextSiblingElement();
	GUARANTEE_OR_DIE(p1UnitsRootElement != nullptr, "Failed to read player 1 units element!");

	tinyxml2::XMLNode const* p1UnitsCDataElement = p1UnitsRootElement->FirstChild();
	GUARANTEE_OR_DIE(p1UnitsCDataElement != nullptr, "Failed to read player 1 units cdata!");
	std::string p1UnitsCData = p1UnitsCDataElement->Value();

	Strings p1UnitDefRows = SplitStringOnDelimiter(p1UnitsCData, '\n');
	for (int stringIndex = static_cast<int>(p1UnitDefRows.size()) - 1; stringIndex > -1; stringIndex--)	//start from the bottom
	{
		Strings unitDefRow = SplitStringOnDelimiter(p1UnitDefRows[stringIndex], ' ');

		for (int charIndex = 0; charIndex < unitDefRow.size(); charIndex++)
		{
			if (!unitDefRow[charIndex].empty())
			{
				std::string unitDefSymbol = unitDefRow[charIndex];
				m_p1UnitDefs.emplace_back(unitDefSymbol);
			}
		}
	}

	XmlElement const* p2UnitsRootElement = p1UnitsRootElement->NextSiblingElement();
	GUARANTEE_OR_DIE(p2UnitsRootElement != nullptr, "Failed to read player 2 units element!");

	tinyxml2::XMLNode const* p2UnitsCDataElement = p2UnitsRootElement->FirstChild();
	GUARANTEE_OR_DIE(p2UnitsCDataElement != nullptr, "Failed to read player 2 units cdata!");
	std::string p2UnitsCData = p2UnitsCDataElement->Value();

	Strings p2UnitDefRows = SplitStringOnDelimiter(p2UnitsCData, '\n');
	for (int stringIndex = static_cast<int>(p2UnitDefRows.size()) - 1; stringIndex > -1; stringIndex--)	//start from the bottom
	{
		Strings unitDefRow = SplitStringOnDelimiter(p2UnitDefRows[stringIndex], ' ');

		for (int charIndex = 0; charIndex < unitDefRow.size(); charIndex++)
		{
			if (!unitDefRow[charIndex].empty())
			{
				std::string unitDefSymbol = unitDefRow[charIndex];
				m_p2UnitDefs.emplace_back(unitDefSymbol);
			}
		}
	}
}


//
//static functions
//
void MapDefinition::InitializeMapDefs()
{
	XmlDocument mapDefsXml;
	char const* filePath = "Data/Definitions/MapDefinitions.xml";
	XmlError result = mapDefsXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, "Failed to open map definitions xml file!");

	XmlElement* rootElement = mapDefsXml.RootElement();
	GUARANTEE_OR_DIE(rootElement != nullptr, "Failed to read map definitions root element!");

	XmlElement* mapDefElement = rootElement->FirstChildElement();
	while (mapDefElement != nullptr)
	{
		std::string elementName = mapDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "MapDefinition", "Child element names in map definitions xml file must be <MapDefinition>!");
		MapDefinition newMapDef = MapDefinition(*mapDefElement);
		s_mapDefinitions.push_back(newMapDef);
		mapDefElement = mapDefElement->NextSiblingElement();
	}
}


MapDefinition const* MapDefinition::GetMapDefinition(std::string name)
{
	for (int defIndex = 0; defIndex < s_mapDefinitions.size(); defIndex++)
	{
		if (s_mapDefinitions[defIndex].m_name == name)
		{
			return &s_mapDefinitions[defIndex];
		}
	}

	//return null if it wasn't found
	return nullptr;
}
