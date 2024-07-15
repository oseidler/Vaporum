#pragma once
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"


//forward declarations
class Shader;


class MapDefinition
{
//public member variables
public:
	static std::vector<MapDefinition> s_mapDefinitions;

	//map parameters
	std::string m_name = "invalid map";
	IntVec2		m_gridSize = IntVec2();
	Vec3		m_boundsMin = Vec3();
	Vec3		m_boundsMax = Vec3();
	Shader*		m_overlayShader = nullptr;
	Strings		m_tileDefs;
	Strings		m_p1UnitDefs;
	Strings		m_p2UnitDefs;

//public member functions
public:
	//constructor
	explicit MapDefinition(XmlElement const& element);

	//static functions
	static void InitializeMapDefs();
	static MapDefinition const* GetMapDefinition(std::string name);
};
