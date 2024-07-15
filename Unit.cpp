#include "Game/Unit.hpp"
#include "Game/Model.hpp"
#include "Game/Game.hpp"
#include "Game/Tile.hpp"
#include "Game/Map.hpp"
#include "Game/GameCommon.hpp"
#include "Engine/Renderer/Renderer.hpp"


//constructor
Unit::Unit(UnitDefinition const* definition, IntVec2 startCoords, int ownerID)
	: m_definition(definition)
	, m_coords(startCoords)
	, m_ownerID(ownerID)
{
	m_currentHealth = m_definition->m_health;
}


//game flow functions
void Unit::Render() const
{
	if (m_definition != nullptr && m_definition->m_model != nullptr)
	{
		Tile const& tile = g_theGame->m_currentMap->m_tiles[g_theGame->m_currentMap->GetTileIndex(m_coords)];
		Vec3 position = tile.GetCenterPos();

		EulerAngles orientation = EulerAngles();
		Rgba8 color = Rgba8(150, 150, 255);
		if (m_ownerID == 2)
		{
			//orientation = EulerAngles(180.0f, 0.0f, 0.0f);
			color = Rgba8(255, 150, 150);
		}

		if (m_movedThisTurn)
		{
			color = Rgba8(150, 150, 150);
		}

		m_definition->m_model->RenderGPUMesh(g_theGame->m_sunDirection, g_theGame->m_sunIntensity, g_theGame->m_ambientIntensity, position, orientation, color);
	}
}
