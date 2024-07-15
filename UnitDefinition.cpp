#include "Game/UnitDefinition.hpp"
#include "Game/Model.hpp"


//static variable declaration
std::vector<UnitDefinition> UnitDefinition::s_unitDefinitions;


//
//constructor and destructor
//
UnitDefinition::UnitDefinition(XmlElement const& element)
{
	m_name = ParseXmlAttribute(element, "name", m_name);
	m_symbol = ParseXmlAttribute(element, "symbol", m_symbol);
	m_groundAttackDamage = ParseXmlAttribute(element, "groundAttackDamage", m_groundAttackDamage);
	m_groundAttackRangeMin = ParseXmlAttribute(element, "groundAttackRangeMin", m_groundAttackRangeMin);
	m_groundAttackRangeMax = ParseXmlAttribute(element, "groundAttackRangeMax", m_groundAttackRangeMax);
	m_movementRange = ParseXmlAttribute(element, "movementRange", m_groundAttackRangeMax);
	m_defense = ParseXmlAttribute(element, "defense", m_defense);
	m_health = ParseXmlAttribute(element, "health", m_health);

	m_model = new Model();
	std::string modelFileName = ParseXmlAttribute(element, "modelFilename", "invalid file name");
	bool success = m_model->ParseXMLFileForOBJ(modelFileName);
	if (!success)
	{
		std::string err = Stringf("Couldn't successfully load model for %s! File path: %s", m_name.c_str(), modelFileName.c_str());
		ERROR_RECOVERABLE(err);
	}

	std::string type = ParseXmlAttribute(element, "type", "Invalid");
	if (type == "Tank")
	{
		m_type = UnitType::TANK;
	}
	else if (type == "Artillery")
	{
		m_type = UnitType::ARTILLERY;
	}
}


//
//static functions
//
void UnitDefinition::InitializeUnitDefs()
{
	XmlDocument unitDefsXml;
	char const* filePath = "Data/Definitions/UnitDefinitions.xml";
	XmlError result = unitDefsXml.LoadFile(filePath);
	GUARANTEE_OR_DIE(result == tinyxml2::XML_SUCCESS, "Failed to open unit definitions xml file!");

	XmlElement* rootElement = unitDefsXml.RootElement();
	GUARANTEE_OR_DIE(rootElement != nullptr, "Failed to read unit definitions root element!");

	XmlElement* unitDefElement = rootElement->FirstChildElement();
	while (unitDefElement != nullptr)
	{
		std::string elementName = unitDefElement->Name();
		GUARANTEE_OR_DIE(elementName == "UnitDefinition", "Child element names in unit definitions xml file must be <UnitDefinition>!");
		UnitDefinition newUnitDef = UnitDefinition(*unitDefElement);
		s_unitDefinitions.push_back(newUnitDef);
		unitDefElement = unitDefElement->NextSiblingElement();
	}
}


UnitDefinition const* UnitDefinition::GetUnitDefinitionByName(std::string name)
{
	for (int defIndex = 0; defIndex < s_unitDefinitions.size(); defIndex++)
	{
		if (s_unitDefinitions[defIndex].m_name == name)
		{
			return &s_unitDefinitions[defIndex];
		}
	}

	//return null if it wasn't found
	return nullptr;
}


UnitDefinition const* UnitDefinition::GetUnitDefinitionBySymbol(std::string symbol)
{
	if (symbol.empty())
	{
		return nullptr;
	}

	for (int defIndex = 0; defIndex < s_unitDefinitions.size(); defIndex++)
	{
		if (s_unitDefinitions[defIndex].m_symbol == symbol.front())
		{
			return &s_unitDefinitions[defIndex];
		}
	}

	//return null if it wasn't found
	return nullptr;
}
