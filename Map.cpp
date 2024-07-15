#include "Game/Map.hpp"
#include "Game/Game.hpp"
#include "Game/GameCamera.hpp"
#include "Game/GameCommon.hpp"
#include "Game/App.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/Shader.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Window/Window.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"


//
//constructor and destructor
//
Map::Map(MapDefinition const* definition)
	: m_definition(definition)
	, m_distanceFieldFromSelectedUnit(TileHeatMap(definition->m_gridSize))
{
	//create tiles and units
	for (int gridYIndex = 0; gridYIndex < m_definition->m_gridSize.y; gridYIndex++)
	{
		for (int gridXIndex = 0; gridXIndex < m_definition->m_gridSize.x; gridXIndex++)
		{
			//create tile at coordinates
			IntVec2 coords = IntVec2(gridXIndex, gridYIndex);
			int defIndex = gridYIndex * m_definition->m_gridSize.x + gridXIndex;
			if (defIndex > m_definition->m_tileDefs.size())
			{
				ERROR_AND_DIE("Map Def Grid Size and Tile Def Mismatch!");
			}

			TileDefinition const* tileDef = TileDefinition::GetTileDefinitionBySymbol(m_definition->m_tileDefs[defIndex]);
			if (tileDef == nullptr)
			{
				tileDef = TileDefinition::GetTileDefinitionByName("Blocked");
			}
			m_tiles.emplace_back(Tile(tileDef, coords));

			//create player 1 unit at coordinates
			UnitDefinition const* p1unitDef = UnitDefinition::GetUnitDefinitionBySymbol(m_definition->m_p1UnitDefs[defIndex]);
			if (p1unitDef != nullptr)
			{
				m_player1Units.emplace_back(Unit(p1unitDef, coords, 1));
			}

			//create player 2 unit at coordinates
			UnitDefinition const* p2unitDef = UnitDefinition::GetUnitDefinitionBySymbol(m_definition->m_p2UnitDefs[defIndex]);
			if (p2unitDef != nullptr)
			{
				m_player2Units.emplace_back(Unit(p2unitDef, coords, 2));
			}
		}
	}

	//create vertex and index buffers for tiles
	m_tileVertBuffer = g_theRenderer->CreateVertexBuffer(sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN));
	m_tileIndexBuffer = g_theRenderer->CreateIndexBuffer(sizeof(unsigned int));

	//add verts for all tiles
	for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
	{
		Tile& tile = m_tiles[tileIndex];
		//check if outside of world bounds or blocked
		if (!IsTileSelectable(tile))
		{
			if (IsTileInBounds(tile) && tile.m_definition->m_isBlocked)
			{
				tile.AddVertsForBlockedTile(m_tileVerts, m_tileVertIndexes);
			}
			else
			{
				continue;
			}
		}
		else
		{
			tile.AddVertsForTile(m_tileVerts, m_tileVertIndexes);
		}
	}

	g_theRenderer->CopyCPUToGPU(m_tileVerts.data(), static_cast<int>(m_tileVerts.size()) * sizeof(Vertex_PCUTBN), m_tileVertBuffer);
	g_theRenderer->CopyCPUToGPU(m_tileVertIndexes.data(), static_cast<int>(m_tileVertIndexes.size()) * sizeof(unsigned int), m_tileIndexBuffer);

	//subscribe to network events
	SubscribeEventCallbackFunction("StartTurn", Event_StartTurn);
	SubscribeEventCallbackFunction("SelectHex", Event_SelectHex);
	SubscribeEventCallbackFunction("SelectUnit", Event_SelectUnit);
	SubscribeEventCallbackFunction("SelectFirstUnit", Event_SelectFirstUnit);
	SubscribeEventCallbackFunction("SelectLastUnit", Event_SelectLastUnit);
	SubscribeEventCallbackFunction("SelectPreviousUnit", Event_SelectPreviousUnit);
	SubscribeEventCallbackFunction("SelectNextUnit", Event_SelectNextUnit);
	SubscribeEventCallbackFunction("MoveUnit", Event_MoveUnit);
	SubscribeEventCallbackFunction("ConfirmMove", Event_ConfirmMove);
	SubscribeEventCallbackFunction("Attack", Event_Attack);
	SubscribeEventCallbackFunction("ConfirmAttack", Event_ConfirmAttack);
	SubscribeEventCallbackFunction("CancelMove", Event_CancelMove);
	SubscribeEventCallbackFunction("EndTurn", Event_EndTurn);
	SubscribeEventCallbackFunction("ConfirmEnd", Event_ConfirmEnd);
	SubscribeEventCallbackFunction("CancelEnd", Event_CancelEnd);
}


Map::~Map()
{
	//delete allocated pointers
	if (m_tileVertBuffer != nullptr)
	{
		delete m_tileVertBuffer;
		m_tileVertBuffer = nullptr;
	}

	if (m_tileIndexBuffer != nullptr)
	{
		delete m_tileIndexBuffer;
		m_tileIndexBuffer = nullptr;
	}
}


//
//game flow functions
//
void Map::Update()
{	
	//pause if P is pressed
	if (g_theInput->WasKeyJustPressed('P'))
	{
		g_theGame->EnterPauseMenu();
	}

	//do mouse raycast here
	Vec3 mousePositionInWorld = PerformMouseRaycast();
	mousePositionInWorld.z = 0.0f; //prevent flickering to slight -0.0f value

	std::string posMessage = Stringf("Mouse position: %.2f, %.2f, %.2f (Grid: %i, %i)", mousePositionInWorld.x, mousePositionInWorld.y, mousePositionInWorld.z, m_selectedTileCoords.x,
		m_selectedTileCoords.y);
	DebugAddMessage(posMessage, 0.0f);

	//don't update if it's not the networked player's turn
	if (g_theGame->m_playerID != 0 && g_theGame->m_playerID != m_currentPlayerTurn)
	{
		if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
		{
			g_theGame->EnterPauseMenu();
		}

		switch (m_playerState)
		{
			case PlayerState::UNIT_SELECTED:
			{
				m_previousUnitTileCoords = m_selectedUnit->m_coords;
				PopulateDistanceField(m_distanceFieldFromSelectedUnit, m_selectedUnit->m_coords);
				break;
			}
			case PlayerState::UNIT_MOVE_CONFIRMED:
			{
				PopulateDistanceField(m_distanceFieldFromSelectedUnit, m_selectedUnit->m_coords);
				break;
			}
			case PlayerState::WAITING:
			{
				m_playerState = PlayerState::READY;
				break;
			}
		}
		
		return;
	}

	//check if inside tile
	m_selectedTileCoords = IntVec2(-1, -1);
	bool selectedTileSent = false;
	for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
	{
		if (IsTileSelectable(m_tiles[tileIndex]) && m_tiles[tileIndex].IsPointInsideTile(mousePositionInWorld))
		{
			EventArgs args;
			std::string tileIndexStr = Stringf("%i", tileIndex);
			args.SetValue("TileIndex", tileIndexStr);
			Map::Event_SelectHex(args);
			std::string selectHexCmd = Stringf("SelectHex TileIndex=%i", tileIndex);
			args.SetValue("Command", selectHexCmd);
			FireEvent("RemoteCommand", args);
			selectedTileSent = true;
		}
	}
	if (!selectedTileSent)
	{
		EventArgs args;
		std::string selectHexCmd = Stringf("SelectHex TileIndex=%i", -1);
		args.SetValue("Command", selectHexCmd);
		FireEvent("RemoteCommand", args);
	}

	switch (m_playerState)
	{
		case PlayerState::READY:
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER) || g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				EventArgs args;
				Map::Event_StartTurn(args);
				args.SetValue("Command", "StartTurn");
				FireEvent("RemoteCommand", args);
			}

			break;
		}
		case PlayerState::SELECTING:
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				g_theGame->EnterPauseMenu();
			}
			if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER))
			{
				EventArgs args;
				Event_EndTurn(args);
				args.SetValue("Command", "EndTurn");
				FireEvent("RemoteCommand", args);
			}

			if (g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				EventArgs args;
				Map::Event_SelectUnit(args);
				args.SetValue("Command", "SelectUnit");
				FireEvent("RemoteCommand", args);
			}

			if (g_theInput->WasKeyJustPressed('K'))
			{
				Unit* unit = GetUnitAtCoords(m_selectedTileCoords, 0);
				if (unit != nullptr)
				{
					KillUnit(unit);
				}
			}

			if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT))
			{
				EventArgs args;
				Event_SelectFirstUnit(args);
				args.SetValue("Command", "SelectFirstUnit");
				FireEvent("RemoteCommand", args);
			}
			if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT))
			{
				EventArgs args;
				Event_SelectLastUnit(args);
				args.SetValue("Command", "SelectLastUnit");
				FireEvent("RemoteCommand", args);
			}

			break;
		}
		case PlayerState::UNIT_SELECTED:
		{
			m_previousUnitTileCoords = m_selectedUnit->m_coords;

			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				RevertOrders();
				break;
			}

			if (m_selectedUnit == nullptr)
			{
				ERROR_RECOVERABLE("Error! In unit selection state with no selected unit. Returning to Selecting state");
				m_playerState = PlayerState::SELECTING;
				break;
			}

			//populate distance map using current unit
			PopulateDistanceField(m_distanceFieldFromSelectedUnit, m_selectedUnit->m_coords);

			if (g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				Unit* unit = GetUnitAtCoords(m_selectedTileCoords, m_currentPlayerTurn);
				Unit* enemyUnit = nullptr;
				if (m_currentPlayerTurn == 1)
				{
					enemyUnit = GetUnitAtCoords(m_selectedTileCoords, 2);
				}
				else
				{
					enemyUnit = GetUnitAtCoords(m_selectedTileCoords, 1);
				}
				
				if (unit != nullptr && !unit->m_movedThisTurn)
				{
					m_selectedUnit = unit;

					EventArgs args;
					args.SetValue("Command", "SelectUnit");
					FireEvent("RemoteCommand", args);
				}
				else if (enemyUnit != nullptr)
				{
					//if enemy is within attack range, attack
					int tileIndex = GetTileIndex(enemyUnit->m_coords);
					float distFromUnit = m_distanceFieldFromSelectedUnit.m_values[tileIndex];
					UnitDefinition const* def = m_selectedUnit->m_definition;

					if (distFromUnit <= static_cast<float>(def->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin))
					{
						m_targetedUnit = enemyUnit;
						m_playerState = PlayerState::UNIT_ATTACKING;
					}
				}
				else
				{
					AttemptMove();
				}
			}

			if (g_theInput->WasKeyJustPressed(KEYCODE_RIGHT))
			{
				EventArgs args;
				Event_SelectNextUnit(args);
				args.SetValue("Command", "SelectNextUnit");
				FireEvent("RemoteCommand", args);
			}
			if (g_theInput->WasKeyJustPressed(KEYCODE_LEFT))
			{
				EventArgs args;
				Event_SelectPreviousUnit(args);
				args.SetValue("Command", "SelectPreviousUnit");
				FireEvent("RemoteCommand", args);
			}

			break;
		}
		case PlayerState::UNIT_MOVED:
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				RevertOrders();
			}

			if (g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				if (m_selectedTileCoords == m_selectedUnit->m_coords)
				{
					ConfirmMove();
				}
			}

			break;
		}
		case PlayerState::UNIT_MOVE_CONFIRMED:
		{
			PopulateDistanceField(m_distanceFieldFromSelectedUnit, m_selectedUnit->m_coords);
			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				RevertOrders();
			}

			if (g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				AttemptAttack();
			}

			break;
		}
		case PlayerState::UNIT_ATTACKING:
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				RevertOrders();
			}
			if (g_theInput->WasKeyJustPressed(KEYCODE_LMB))
			{
				if (m_selectedTileCoords == m_targetedUnit->m_coords)
				{
					AttackTargetedUnit();
				}
			}

			break;
		}
		case PlayerState::ENDING_TURN:
		{
			if (g_theInput->WasKeyJustPressed(KEYCODE_ENTER))
			{
				EndTurn();
			}
			if (g_theInput->WasKeyJustPressed(KEYCODE_ESC))
			{
				EventArgs args;
				Event_CancelEnd(args);
				args.SetValue("Command", "CancelEnd");
				FireEvent("RemoteCommand", args);
			}
			break;
		}
		case PlayerState::WAITING:
		{
			m_playerState = PlayerState::READY;
			break;
		}
	}
}


void Map::Render() const
{
	//draw moon texture on ground
	std::vector<Vertex_PCU> groundVerts;
	Vec3 worldBoundsMin = m_definition->m_boundsMin;
	Vec3 worldBoundsMax = m_definition->m_boundsMax;
	float groundBounds = 40.0f;
	Vec3 bottomLeft = Vec3(worldBoundsMin.x - groundBounds, worldBoundsMax.y - groundBounds, 0.0f);
	Vec3 bottomRight = Vec3(worldBoundsMax.x + groundBounds, worldBoundsMin.y - groundBounds, 0.0f);
	Vec3 topLeft = Vec3(worldBoundsMin.x - groundBounds, worldBoundsMax.y + groundBounds, 0.0f);
	Vec3 topRight = Vec3(worldBoundsMax.x + groundBounds, worldBoundsMax.y + groundBounds, 0.0f);
	AddVertsForQuad3D(groundVerts, bottomLeft, bottomRight, topLeft, topRight);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(g_theGame->m_ground);
	g_theRenderer->SetModelConstants();
	g_theRenderer->DrawVertexArray(groundVerts);
	
	//render all tiles
	g_theRenderer->BindTexture(nullptr);
	g_theRenderer->BindShader(m_definition->m_overlayShader);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->SetModelConstants();
	g_theRenderer->SetDepthMode(DepthMode::DISABLED); //to make it render over everything else

	g_theRenderer->DrawVertexBufferIndexed(m_tileVertBuffer, m_tileIndexBuffer, static_cast<int>(m_tileVertIndexes.size()));

	std::string selectedTileMes = Stringf("Selected Tile: %i, %i", m_selectedTileCoords.x, m_selectedTileCoords.y);
	DebugAddMessage(selectedTileMes, 0.0f);

	//render currently selected tile
	if (m_selectedTileCoords != IntVec2(-1, -1))
	{
		std::vector<Vertex_PCUTBN> selectedTileVerts;
		int selectedTileIndex = GetTileIndex(m_selectedTileCoords);
		m_tiles[selectedTileIndex].AddVertsForSelectedTile(selectedTileVerts);
		g_theRenderer->DrawVertexArray(selectedTileVerts);
	}

	switch (m_playerState)
	{
		case PlayerState::READY:
		{

			break;
		}
		case PlayerState::SELECTING:
		{
			
			break;
		}
		case PlayerState::UNIT_SELECTED:
		{
			//render tiles for currently selected unit's range
			if (m_selectedUnit != nullptr)
			{
				std::vector<int> tilesOnPath;
				UnitDefinition const* def = m_selectedUnit->m_definition;
				std::vector<Vertex_PCUTBN> selectedUnitTileVerts;

				int selectedUnitTileIndex = GetTileIndex(m_selectedUnit->m_coords);
				tilesOnPath.emplace_back(selectedUnitTileIndex);
				m_tiles[selectedUnitTileIndex].AddVertsForSelectedUnit(selectedUnitTileVerts);
				int selectedTileIndex = GetTileIndex(m_selectedTileCoords);

				
				float selctedDistFromUnit; 
				if (selectedTileIndex < 0 || selectedTileIndex >= m_tiles.size())
				{
					selctedDistFromUnit = 999.0f;
				}
				else
				{
					selctedDistFromUnit = m_distanceFieldFromSelectedUnit.m_values[selectedTileIndex];
				}

				if (selctedDistFromUnit <= static_cast<float>(def->m_movementRange) && IsTileSelectable(m_tiles[selectedTileIndex]))
				{
					//tilesOnPath.emplace_back(selectedTileIndex);

					int currentTileIndex = selectedTileIndex;
					while (m_distanceFieldFromSelectedUnit.m_values[currentTileIndex] != 0.0f)
					{
						//check all neighbors for lowest cost
						IntVec2 tileCoords = m_tiles[currentTileIndex].m_coords;
						float currentLowestCost = m_distanceFieldFromSelectedUnit.m_values[currentTileIndex];
						int nextTileIndex = currentTileIndex;

						//north tile
						{
							IntVec2 northCoords = IntVec2(tileCoords.x, tileCoords.y + 1);
							int northTileID = GetTileIndex(northCoords);
							if (northTileID < m_tiles.size() && northCoords.x >= 0 && northCoords.y >= 0 && northCoords.x < m_definition->m_gridSize.x && northCoords.y < m_definition->m_gridSize.y)
							{
								float northTileValue = m_distanceFieldFromSelectedUnit.m_values[northTileID];

								if (IsTileSelectable(m_tiles[northTileID]) && (northTileValue < currentLowestCost))
								{
									currentLowestCost = northTileValue;
									nextTileIndex = northTileID;
								}
							}
						}

						//northeast tile
						{
							IntVec2 northeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y);
							int northeastTileID = GetTileIndex(northeastCoords);
							if (northeastTileID < m_tiles.size() && northeastCoords.x >= 0 && northeastCoords.y >= 0 && northeastCoords.x < m_definition->m_gridSize.x && northeastCoords.y < m_definition->m_gridSize.y)
							{
								float northeastTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(northeastTileID);

								if (IsTileSelectable(m_tiles[northeastTileID]) && (northeastTileValue < currentLowestCost))
								{
									currentLowestCost = northeastTileValue;
									nextTileIndex = northeastTileID;
								}
							}
						}

						//southeast tile
						{
							IntVec2 southeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y - 1);
							int southeastTileID = GetTileIndex(southeastCoords);
							if (southeastTileID < m_tiles.size() && southeastCoords.x >= 0 && southeastCoords.y >= 0 && southeastCoords.x < m_definition->m_gridSize.x && southeastCoords.y < m_definition->m_gridSize.y)
							{
								float southeastTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southeastTileID);

								if (IsTileSelectable(m_tiles[southeastTileID]) && (southeastTileValue < currentLowestCost))
								{
									currentLowestCost = southeastTileValue;
									nextTileIndex = southeastTileID;
								}
							}
						}

						//south tile
						{
							IntVec2 southCoords = IntVec2(tileCoords.x, tileCoords.y - 1);
							int southTileID = GetTileIndex(southCoords);
							if (southTileID < m_tiles.size() && southCoords.x >= 0 && southCoords.y >= 0 && southCoords.x < m_definition->m_gridSize.x && southCoords.y < m_definition->m_gridSize.y)
							{
								float southTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southTileID);

								if (IsTileSelectable(m_tiles[southTileID]) && (southTileValue < currentLowestCost))
								{
									currentLowestCost = southTileValue;
									nextTileIndex = southTileID;
								}
							}
						}

						//southwest tile
						{
							IntVec2 southwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y);
							int southwestTileID = GetTileIndex(southwestCoords);
							if (southwestTileID < m_tiles.size() && southwestCoords.x >= 0 && southwestCoords.y >= 0 && southwestCoords.x < m_definition->m_gridSize.x && southwestCoords.y < m_definition->m_gridSize.y)
							{
								float southwestTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southwestTileID);

								if (IsTileSelectable(m_tiles[southwestTileID]) && (southwestTileValue < currentLowestCost))
								{
									currentLowestCost = southwestTileValue;
									nextTileIndex = southwestTileID;
								}
							}
						}

						//northwest tile
						{
							IntVec2 northwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y + 1);
							int northwestTileID = GetTileIndex(northwestCoords);
							if (northwestTileID < m_tiles.size() && northwestCoords.x >= 0 && northwestCoords.y >= 0 && northwestCoords.x < m_definition->m_gridSize.x && northwestCoords.y < m_definition->m_gridSize.y)
							{
								float northwestTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(northwestTileID);

								if (IsTileSelectable(m_tiles[northwestTileID]) && (northwestTileValue < currentLowestCost))
								{
									currentLowestCost = northwestTileValue;
									nextTileIndex = northwestTileID;
								}
							}
						}
						
						//add that one and set it as the current tile index
						if (nextTileIndex != currentTileIndex)
						{
							tilesOnPath.emplace_back(currentTileIndex);
							currentTileIndex = nextTileIndex;
						}
					}
				}

				for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
				{
					//do distance field stuff here
					Tile const& tile = m_tiles[tileIndex];
					float distFromUnit = m_distanceFieldFromSelectedUnit.m_values[tileIndex];

					std::string distanceFieldValue = Stringf("%.1f", distFromUnit);
					//DebugAddWorldBillboardText(distanceFieldValue, tile.GetCenterPos(), 0.1f, Vec2(0.5f, 0.5f), 0.0f);

					if (distFromUnit <= static_cast<float>(def->m_movementRange) && IsTileSelectable(tile))
					{
						//render highlighted path logic here
						bool isOnPath = false;
						for (int pathIndex = 0; pathIndex < tilesOnPath.size(); pathIndex++)
						{
							if (tilesOnPath[pathIndex] == tileIndex)
							{
								isOnPath = true;
								break;
							}
						}
						
						if (isOnPath)
						{
							tile.AddVertsForTileOnMovePath(selectedUnitTileVerts);
						}
						else
						{
							tile.AddVertsForTileInMoveRange(selectedUnitTileVerts);
						}
					}

					//display red outline on tiles with enemies within attacking range
					Unit* enemyUnit = nullptr;
					if (m_currentPlayerTurn == 1)
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 2);
					}
					else //m_currentPlayerTurn == 2
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 1);
					}

					if (def->m_type == UnitType::ARTILLERY)
					{
						if (enemyUnit != nullptr && distFromUnit <= static_cast<float>(def->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin) &&
							IsTileSelectable(tile))
						{
							tile.AddVertsForTileInAttackRange(selectedUnitTileVerts);
						}
					}
					else if (def->m_type == UnitType::TANK)
					{
						if (enemyUnit != nullptr && distFromUnit <= static_cast<float>(def->m_movementRange + def->m_groundAttackRangeMax) &&
							distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin) && IsTileSelectable(tile))
						{
							tile.AddVertsForTileInAttackRange(selectedUnitTileVerts);
						}
					}
				}

				g_theRenderer->DrawVertexArray(selectedUnitTileVerts);
			}

			break;
		}
		case PlayerState::UNIT_MOVED:
		{
			//render tiles for currently selected unit's range
			if (m_selectedUnit != nullptr)
			{
				std::vector<int> tilesOnPath;
				UnitDefinition const* def = m_selectedUnit->m_definition;
				std::vector<Vertex_PCUTBN> selectedUnitTileVerts;

				int selectedUnitTileIndex = GetTileIndex(m_selectedUnit->m_coords);
				int previousUnitTileIndex = GetTileIndex(m_previousUnitTileCoords);
				tilesOnPath.emplace_back(previousUnitTileIndex);
				m_tiles[selectedUnitTileIndex].AddVertsForSelectedUnit(selectedUnitTileVerts);

				float selctedDistFromUnit;
				if (selectedUnitTileIndex < 0 || selectedUnitTileIndex >= m_tiles.size())
				{
					selctedDistFromUnit = 999.0f;
				}
				else
				{
					selctedDistFromUnit = m_distanceFieldFromSelectedUnit.m_values[selectedUnitTileIndex];
				}

				if (selctedDistFromUnit <= static_cast<float>(def->m_movementRange) && IsTileSelectable(m_tiles[selectedUnitTileIndex]))
				{
					//tilesOnPath.emplace_back(selectedTileIndex);

					int currentTileIndex = selectedUnitTileIndex;
					while (m_distanceFieldFromSelectedUnit.m_values[currentTileIndex] != 0.0f)
					{
						//check all neighbors for lowest cost
						IntVec2 tileCoords = m_tiles[currentTileIndex].m_coords;
						float currentLowestCost = m_distanceFieldFromSelectedUnit.m_values[currentTileIndex];
						int nextTileIndex = currentTileIndex;

						//north tile
						{
							IntVec2 northCoords = IntVec2(tileCoords.x, tileCoords.y + 1);
							int northTileID = GetTileIndex(northCoords);
							if (northTileID < m_tiles.size() && northCoords.x >= 0 && northCoords.y >= 0 && northCoords.x < m_definition->m_gridSize.x && northCoords.y < m_definition->m_gridSize.y)
							{
								float northTileValue = m_distanceFieldFromSelectedUnit.m_values[northTileID];

								if (IsTileSelectable(m_tiles[northTileID]) && (northTileValue < currentLowestCost))
								{
									currentLowestCost = northTileValue;
									nextTileIndex = northTileID;
								}
							}
						}

						//northeast tile
						{
							IntVec2 northeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y);
							int northeastTileID = GetTileIndex(northeastCoords);
							if (northeastTileID < m_tiles.size() && northeastCoords.x >= 0 && northeastCoords.y >= 0 && northeastCoords.x < m_definition->m_gridSize.x && northeastCoords.y < m_definition->m_gridSize.y)
							{
								float northeastTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(northeastTileID);

								if (IsTileSelectable(m_tiles[northeastTileID]) && (northeastTileValue < currentLowestCost))
								{
									currentLowestCost = northeastTileValue;
									nextTileIndex = northeastTileID;
								}
							}
						}

						//southeast tile
						{
							IntVec2 southeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y - 1);
							int southeastTileID = GetTileIndex(southeastCoords);
							if (southeastTileID < m_tiles.size() && southeastCoords.x >= 0 && southeastCoords.y >= 0 && southeastCoords.x < m_definition->m_gridSize.x && southeastCoords.y < m_definition->m_gridSize.y)
							{
								float southeastTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southeastTileID);

								if (IsTileSelectable(m_tiles[southeastTileID]) && (southeastTileValue < currentLowestCost))
								{
									currentLowestCost = southeastTileValue;
									nextTileIndex = southeastTileID;
								}
							}
						}

						//south tile
						{
							IntVec2 southCoords = IntVec2(tileCoords.x, tileCoords.y - 1);
							int southTileID = GetTileIndex(southCoords);
							if (southTileID < m_tiles.size() && southCoords.x >= 0 && southCoords.y >= 0 && southCoords.x < m_definition->m_gridSize.x && southCoords.y < m_definition->m_gridSize.y)
							{
								float southTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southTileID);

								if (IsTileSelectable(m_tiles[southTileID]) && (southTileValue < currentLowestCost))
								{
									currentLowestCost = southTileValue;
									nextTileIndex = southTileID;
								}
							}
						}

						//southwest tile
						{
							IntVec2 southwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y);
							int southwestTileID = GetTileIndex(southwestCoords);
							if (southwestTileID < m_tiles.size() && southwestCoords.x >= 0 && southwestCoords.y >= 0 && southwestCoords.x < m_definition->m_gridSize.x && southwestCoords.y < m_definition->m_gridSize.y)
							{
								float southwestTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(southwestTileID);

								if (IsTileSelectable(m_tiles[southwestTileID]) && (southwestTileValue < currentLowestCost))
								{
									currentLowestCost = southwestTileValue;
									nextTileIndex = southwestTileID;
								}
							}
						}

						//northwest tile
						{
							IntVec2 northwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y + 1);
							int northwestTileID = GetTileIndex(northwestCoords);
							if (northwestTileID < m_tiles.size() && northwestCoords.x >= 0 && northwestCoords.y >= 0 && northwestCoords.x < m_definition->m_gridSize.x && northwestCoords.y < m_definition->m_gridSize.y)
							{
								float northwestTileValue = m_distanceFieldFromSelectedUnit.GetValueAtTileID(northwestTileID);

								if (IsTileSelectable(m_tiles[northwestTileID]) && (northwestTileValue < currentLowestCost))
								{
									currentLowestCost = northwestTileValue;
									nextTileIndex = northwestTileID;
								}
							}
						}

						//add that one and set it as the current tile index
						if (nextTileIndex != currentTileIndex)
						{
							tilesOnPath.emplace_back(currentTileIndex);
							currentTileIndex = nextTileIndex;
						}
					}
				}

				for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
				{
					//do distance field stuff here
					Tile const& tile = m_tiles[tileIndex];
					float distFromUnit = m_distanceFieldFromSelectedUnit.m_values[tileIndex];

					std::string distanceFieldValue = Stringf("%.1f", distFromUnit);
					//DebugAddWorldBillboardText(distanceFieldValue, tile.GetCenterPos(), 0.1f, Vec2(0.5f, 0.5f), 0.0f);

					if (distFromUnit <= static_cast<float>(def->m_movementRange) && IsTileSelectable(tile))
					{
						//render highlighted path logic here
						bool isOnPath = false;
						for (int pathIndex = 0; pathIndex < tilesOnPath.size(); pathIndex++)
						{
							if (tilesOnPath[pathIndex] == tileIndex)
							{
								isOnPath = true;
								break;
							}
						}

						if (isOnPath)
						{
							tile.AddVertsForTileOnMovePath(selectedUnitTileVerts);
						}
						else
						{
							tile.AddVertsForTileInMoveRange(selectedUnitTileVerts);
						}
					}

					//display red outline on tiles with enemies within attacking range
					Unit* enemyUnit = nullptr;
					if (m_currentPlayerTurn == 1)
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 2);
					}
					else //m_currentPlayerTurn == 2
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 1);
					}

					if (def->m_type == UnitType::ARTILLERY)
					{
						if (enemyUnit != nullptr && distFromUnit <= static_cast<float>(def->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin) && 
							IsTileSelectable(tile))
						{
							tile.AddVertsForTileInAttackRange(selectedUnitTileVerts);
						}
					}
					else if (def->m_type == UnitType::TANK)
					{
						if (enemyUnit != nullptr && distFromUnit <= static_cast<float>(def->m_movementRange + def->m_groundAttackRangeMax) && 
							distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin) && IsTileSelectable(tile))
						{
							tile.AddVertsForTileInAttackRange(selectedUnitTileVerts);
						}
					}
				}

				g_theRenderer->DrawVertexArray(selectedUnitTileVerts);
			}

			break;
		}
		case PlayerState::UNIT_MOVE_CONFIRMED:
		{
			if (m_selectedUnit != nullptr)
			{
				std::vector<Vertex_PCUTBN> selectedUnitTileVerts;
				int selectedTileIndex = GetTileIndex(m_selectedUnit->m_coords);
				m_tiles[selectedTileIndex].AddVertsForSelectedUnit(selectedUnitTileVerts);

				for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
				{
					//do distance field stuff here
					Tile const& tile = m_tiles[tileIndex];
					float distFromUnit = m_distanceFieldFromSelectedUnit.m_values[tileIndex];
					UnitDefinition const* def = m_selectedUnit->m_definition;

					//display red outline on tiles with enemies within attacking range
					Unit* enemyUnit = nullptr;
					if (m_currentPlayerTurn == 1)
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 2);
					}
					else //m_currentPlayerTurn == 2
					{
						enemyUnit = GetUnitAtCoords(tile.m_coords, 1);
					}

					if (enemyUnit != nullptr && distFromUnit <= static_cast<float>(def->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin) &&
						IsTileSelectable(tile))
					{
						tile.AddVertsForTileInAttackRange(selectedUnitTileVerts);
					}
				}

				g_theRenderer->DrawVertexArray(selectedUnitTileVerts);
			}

			break;
		}
		case PlayerState::UNIT_ATTACKING:
		{
			if (m_selectedUnit != nullptr)
			{
				std::vector<Vertex_PCUTBN> selectedUnitTileVerts;
				int selectedTileIndex = GetTileIndex(m_selectedUnit->m_coords);
				m_tiles[selectedTileIndex].AddVertsForSelectedUnit(selectedUnitTileVerts);

				if (m_targetedUnit != nullptr)
				{
					int tileIndex = GetTileIndex(m_targetedUnit->m_coords);
					m_tiles[tileIndex].AddVertsForTileBeingAttacked(selectedUnitTileVerts);
				}

				g_theRenderer->DrawVertexArray(selectedUnitTileVerts);
			}

			break;
		}
		case PlayerState::ENDING_TURN:
		{
			
			break;
		}
		case PlayerState::WAITING:
		{
			
			break;
		}
	}

	g_theRenderer->SetDepthMode(DepthMode::ENABLED); //fix the depth mode since I'd def forget to set it back elsewhere

	//render unit models
	for (int unitIndex = 0; unitIndex < m_player1Units.size(); unitIndex++)
	{
		m_player1Units[unitIndex].Render();
	}
	for (int unitIndex = 0; unitIndex < m_player2Units.size(); unitIndex++)
	{
		m_player2Units[unitIndex].Render();
	}
}


//
//map utilities
//
Vec3 Map::PerformMouseRaycast()
{
	//establish known variables
	Camera const& camera = g_theGame->m_gameCamera->m_camera;
	float nearClipDist = camera.GetPerspectiveNear();
	float aspect = camera.GetPerspectiveAspect();
	float fov = camera.GetPerspectiveFOV();
	Mat44 cameraMatrix = camera.GetViewMatrix().GetOrthonormalInverse();
	Vec3 cameraIBasis = cameraMatrix.GetIBasis3D();
	Vec3 cameraJBasis = cameraMatrix.GetJBasis3D();
	Vec3 cameraKBasis = cameraMatrix.GetKBasis3D();
	Vec3 cameraPosition = camera.GetCameraPosition();

	float screenWidth = static_cast<float>(g_theWindow->GetClientDimensions().x);
	float screenHeight = static_cast<float>(g_theWindow->GetClientDimensions().y);
	IntVec2 mousePosOnScreenInt = g_theInput->GetCursorClientPosition();
	Vec2 mousePosOnScreen = Vec2(static_cast<float>(mousePosOnScreenInt.x), static_cast<float>(mousePosOnScreenInt.y));
	
	Vec2 screenCenter = Vec2(screenWidth * 0.5f, screenHeight * 0.5f);
	float mousePosScreenOffsetX = RangeMapClamped(mousePosOnScreen.x, 0.0f, screenWidth, -screenCenter.x, screenCenter.x);
	mousePosScreenOffsetX /= screenCenter.x;
	float mousePosScreenOffsetY = RangeMapClamped(mousePosOnScreen.y, 0.0f, screenHeight, -screenCenter.y, screenCenter.y);
	mousePosScreenOffsetY /= screenCenter.y;

	//calculate mouse position in world
	Vec3 mouseInWorldXDir = cameraIBasis * nearClipDist;
	Vec3 mouseInWorldYDir = cameraJBasis * -mousePosScreenOffsetX * aspect * nearClipDist * TanDegrees(fov * 0.5f);
	Vec3 mouseInWorldZDir = cameraKBasis * -mousePosScreenOffsetY * nearClipDist * TanDegrees(fov * 0.5f);
	Vec3 mouseDirectionVector = mouseInWorldXDir + mouseInWorldYDir + mouseInWorldZDir;
	mouseDirectionVector.Normalize();
	//Vec3 mousePosInWorld = camera.GetCameraPosition() + mouseInWorldXDir + mouseInWorldYDir + mouseInWorldZDir;

	//do raycast of infinite length from camera position through mouse world position
	Vec3 raycastVector = mouseDirectionVector * FLT_MAX;
	float t = (-cameraPosition.z) / raycastVector.z;
	Vec3 posAtT = cameraPosition + (raycastVector * t);
	return posAtT;
}


int Map::GetTileIndex(IntVec2 tileCoords) const
{
	return tileCoords.y * m_definition->m_gridSize.x + tileCoords.x;
}


bool Map::IsTileInBounds(Tile const& tile) const
{
	float centerXPos = tile.GetCenterPosX();
	float centerYPos = tile.GetCenterPosY();
	return centerXPos > m_definition->m_boundsMin.x && centerXPos < m_definition->m_boundsMax.x && centerYPos > m_definition->m_boundsMin.y && centerYPos < m_definition->m_boundsMax.y;
}


bool Map::IsTileSelectable(Tile const& tile) const
{
	return IsTileInBounds(tile) && !tile.m_definition->m_isBlocked;
}


void Map::EndTurn()
{
	EventArgs args;
	Event_ConfirmEnd(args);
	args.SetValue("Command", "ConfirmEnd");
	FireEvent("RemoteCommand", args);
}


Unit* Map::GetUnitAtCoords(IntVec2 tileCoords, int player) const
{
	if (player == 1)
	{
		for (int unitIndex = 0; unitIndex < m_player1Units.size(); unitIndex++)
		{
			if (m_player1Units[unitIndex].m_coords == tileCoords)
			{
				return &m_player1Units[unitIndex];
			}
		}
	}
	else if (player == 2)
	{
		for (int unitIndex = 0; unitIndex < m_player2Units.size(); unitIndex++)
		{
			if (m_player2Units[unitIndex].m_coords == tileCoords)
			{
				return &m_player2Units[unitIndex];
			}
		}
	}
	else
	{
		for (int unitIndex = 0; unitIndex < m_player1Units.size(); unitIndex++)
		{
			if (m_player1Units[unitIndex].m_coords == tileCoords)
			{
				return &m_player1Units[unitIndex];
			}
		}

		for (int unitIndex = 0; unitIndex < m_player2Units.size(); unitIndex++)
		{
			if (m_player2Units[unitIndex].m_coords == tileCoords)
			{
				return &m_player2Units[unitIndex];
			}
		}
	}

	return nullptr;
}


void Map::RevertOrders()
{
	EventArgs args;
	Event_CancelMove(args);
	args.SetValue("Command", "CancelMove");
	FireEvent("RemoteCommand", args);
}


void Map::AttemptMove()
{
	int selectedTileIndex = GetTileIndex(m_selectedTileCoords);
	if (selectedTileIndex < 0 || selectedTileIndex > m_tiles.size() || !IsTileSelectable(m_tiles[selectedTileIndex]))
	{
		return;
	}
	
	UnitDefinition const* def = m_selectedUnit->m_definition;
	if (m_distanceFieldFromSelectedUnit.m_values[selectedTileIndex] <= static_cast<float>(def->m_movementRange))
	{
		EventArgs args;
		Event_MoveUnit(args);
		args.SetValue("Command", "MoveUnit");
		FireEvent("RemoteCommand", args);
	}
}


void Map::ConfirmMove()
{
	EventArgs args;
	Event_ConfirmMove(args);
	args.SetValue("Command", "ConfirmMove");
	FireEvent("RemoteCommand", args);
}


void Map::AttemptAttack()
{
	EventArgs args;
	Event_Attack(args);
	args.SetValue("Command", "Attack");
	FireEvent("RemoteCommand", args);
}


void Map::AttackTargetedUnit()
{
	EventArgs args;
	Event_ConfirmAttack(args);
	args.SetValue("Command", "ConfirmAttack");
	FireEvent("RemoteCommand", args);
}


void Map::KillUnit(Unit* unit)
{
	if (unit == nullptr) return;

	if (unit->m_ownerID == 1)
	{
		//use erase-remove idiom to remove unit from map
		m_player1Units.erase(std::remove_if(
			m_player1Units.begin(),
			m_player1Units.end(),
			[=](auto const& element)
			{
				return &element == unit;
			}),
			m_player1Units.end()
				);
	}
	else //if unit->m_ownerID == 2
	{
		//use erase-remove idiom to remove unit from map
		m_player2Units.erase(std::remove_if(
			m_player2Units.begin(),
			m_player2Units.end(),
			[=](auto const& element)
			{
				return &element == unit;
			}),
			m_player2Units.end()
				);
	}

	unit = nullptr;
}


void Map::PopulateDistanceField(TileHeatMap& outDistanceField, IntVec2 const& referenceCoords)
{
	//set all values to max cost except at the reference coords, where they're 0.0f
	outDistanceField.SetAllValues(999.0f);
	outDistanceField.SetValueAtTileCoords(0.0f, referenceCoords);

	//do the propogation outwards from the reference coords
	for (int passIndex = 0; passIndex < 999; passIndex++)
	{
		bool wereValuesChanged = false;

		for (int tileIndex = 0; tileIndex < m_tiles.size(); tileIndex++)
		{
			float newValue = static_cast<float>(passIndex + 1);
			float tileValue = outDistanceField.GetValueAtTileID(tileIndex);

			if (tileValue == static_cast<float>(passIndex))
			{
				//set neighboring tiles if they exist, aren't blocked or off-map, and they don't already have a lower value
				IntVec2 tileCoords = m_tiles[tileIndex].m_coords;

				//north tile
				{
					IntVec2 northCoords = IntVec2(tileCoords.x, tileCoords.y + 1);
					int northTileID = GetTileIndex(northCoords);
					if (northTileID < m_tiles.size() && northCoords.x >= 0 && northCoords.y >= 0 && northCoords.x < m_definition->m_gridSize.x && northCoords.y < m_definition->m_gridSize.y)
					{
						float northTileValue = outDistanceField.GetValueAtTileID(northTileID);

						if (IsTileSelectable(m_tiles[northTileID]) && (northTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, northTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}

				//northeast tile
				{
					IntVec2 northeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y);
					int northeastTileID = GetTileIndex(northeastCoords);
					if (northeastTileID < m_tiles.size() && northeastCoords.x >= 0 && northeastCoords.y >= 0 && northeastCoords.x < m_definition->m_gridSize.x && northeastCoords.y < m_definition->m_gridSize.y)
					{
						float northeastTileValue = outDistanceField.GetValueAtTileID(northeastTileID);

						if (IsTileSelectable(m_tiles[northeastTileID]) && (northeastTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, northeastTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}

				//southeast tile
				{
					IntVec2 southeastCoords = IntVec2(tileCoords.x + 1, tileCoords.y - 1);
					int southeastTileID = GetTileIndex(southeastCoords);
					if (southeastTileID < m_tiles.size() && southeastCoords.x >= 0 && southeastCoords.y >= 0 && southeastCoords.x < m_definition->m_gridSize.x && southeastCoords.y < m_definition->m_gridSize.y)
					{
						float southeastTileValue = outDistanceField.GetValueAtTileID(southeastTileID);

						if (IsTileSelectable(m_tiles[southeastTileID]) && (southeastTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, southeastTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}

				//south tile
				{
					IntVec2 southCoords = IntVec2(tileCoords.x, tileCoords.y - 1);
					int southTileID = GetTileIndex(southCoords);
					if (southTileID < m_tiles.size() && southCoords.x >= 0 && southCoords.y >= 0 && southCoords.x < m_definition->m_gridSize.x && southCoords.y < m_definition->m_gridSize.y)
					{
						float southTileValue = outDistanceField.GetValueAtTileID(southTileID);

						if (IsTileSelectable(m_tiles[southTileID]) && (southTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, southTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}

				//southwest tile
				{
					IntVec2 southwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y);
					int southwestTileID = GetTileIndex(southwestCoords);
					if (southwestTileID < m_tiles.size() && southwestCoords.x >= 0 && southwestCoords.y >= 0 && southwestCoords.x < m_definition->m_gridSize.x && southwestCoords.y < m_definition->m_gridSize.y)
					{
						float southwestTileValue = outDistanceField.GetValueAtTileID(southwestTileID);

						if (IsTileSelectable(m_tiles[southwestTileID]) && (southwestTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, southwestTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}

				//northwest tile
				{
					IntVec2 northwestCoords = IntVec2(tileCoords.x - 1, tileCoords.y + 1);
					int northwestTileID = GetTileIndex(northwestCoords);
					if (northwestTileID < m_tiles.size() && northwestCoords.x >= 0 && northwestCoords.y >= 0 && northwestCoords.x < m_definition->m_gridSize.x && northwestCoords.y < m_definition->m_gridSize.y)
					{
						float northwestTileValue = outDistanceField.GetValueAtTileID(northwestTileID);

						if (IsTileSelectable(m_tiles[northwestTileID]) && (northwestTileValue > newValue))
						{
							outDistanceField.SetValueAtTileID(newValue, northwestTileID);

							//set whether any values were changed
							wereValuesChanged = true;
						}
					}
				}
			}
		}

		if (!wereValuesChanged)
		{
			break;	//break out of the loop early if no tiles were changed
		}
	}
}


bool Map::Event_StartTurn(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	g_theGame->m_currentMap->m_playerState = PlayerState::SELECTING;

	return true;
}


bool Map::Event_SelectHex(EventArgs& args)
{
	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}
	
	int tileIndex = args.GetValue("TileIndex", -1);
	
	if (tileIndex != -1)
	{
		g_theGame->m_currentMap->m_selectedTileCoords = g_theGame->m_currentMap->m_tiles[tileIndex].m_coords;
	}
	else
	{
		g_theGame->m_currentMap->m_selectedTileCoords = IntVec2(-1, -1);
	}

	return true;
}


bool Map::Event_SelectUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	//if not selecting an open tile within movement range of currently selected unit, select the unit at the current tile (if there is one of the current player's)
	Unit* unit = theMap->GetUnitAtCoords(theMap->m_selectedTileCoords, theMap->m_currentPlayerTurn);
	if (unit != nullptr && !unit->m_movedThisTurn)
	{
		theMap->m_selectedUnit = unit;
		theMap->m_playerState = PlayerState::UNIT_SELECTED;
	}

	return true;
}


bool Map::Event_SelectFirstUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	theMap->m_playerState = PlayerState::UNIT_SELECTED;
	if (theMap->m_selectedUnit == nullptr)
	{
		if (theMap->m_currentPlayerTurn == 1)
		{
			for (int unitIndex = 0; unitIndex < theMap->m_player1Units.size(); unitIndex++)
			{
				if (!theMap->m_player1Units[unitIndex].m_movedThisTurn)
				{
					theMap->m_selectedUnit = &theMap->m_player1Units[unitIndex];
					break;
				}
			}
		}
		else //if m_currentPlayerTurn == 2
		{
			for (int unitIndex = 0; unitIndex < theMap->m_player2Units.size(); unitIndex++)
			{
				if (!theMap->m_player2Units[unitIndex].m_movedThisTurn)
				{
					theMap->m_selectedUnit = &theMap->m_player2Units[unitIndex];
					break;
				}
			}
		}
	}

	return true;
}


bool Map::Event_SelectLastUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	theMap->m_playerState = PlayerState::UNIT_SELECTED;
	if (theMap->m_selectedUnit == nullptr)
	{
		if (theMap->m_currentPlayerTurn == 1)
		{
			for (int unitIndex = static_cast<int>(theMap->m_player1Units.size()) - 1; unitIndex >= 0; unitIndex--)
			{
				if (!theMap->m_player1Units[unitIndex].m_movedThisTurn)
				{
					theMap->m_selectedUnit = &theMap->m_player1Units[unitIndex];
					break;
				}
			}
		}
		else //if m_currentPlayerTurn == 2
		{
			for (int unitIndex = static_cast<int>(theMap->m_player2Units.size()) - 1; unitIndex >= 0; unitIndex--)
			{
				if (!theMap->m_player2Units[unitIndex].m_movedThisTurn)
				{
					theMap->m_selectedUnit = &theMap->m_player2Units[unitIndex];
					break;
				}
			}
		}
	}

	return true;
}


bool Map::Event_SelectPreviousUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	if (theMap->m_currentPlayerTurn == 1)
	{
		for (int unitIndex = 0; unitIndex < theMap->m_player1Units.size(); unitIndex++)
		{
			if (&theMap->m_player1Units[unitIndex] == theMap->m_selectedUnit)
			{
				if (unitIndex == 0)
				{
					for (int nextUnitIndex = static_cast<int>(theMap->m_player1Units.size() - 1); nextUnitIndex >= 0; nextUnitIndex--)
					{
						if (!theMap->m_player1Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player1Units[nextUnitIndex];
							break;
						}
					}

				}
				else
				{
					for (int nextUnitIndex = unitIndex - 1; nextUnitIndex != unitIndex; nextUnitIndex--)
					{
						if (!theMap->m_player1Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player1Units[nextUnitIndex];
							break;
						}

						if (nextUnitIndex == 0)
						{
							nextUnitIndex = static_cast<int>(theMap->m_player1Units.size());
						}
					}
				}
				break;
			}
		}
	}
	else //if m_currentPlayerTurn == 2
	{
		for (int unitIndex = 0; unitIndex < theMap->m_player2Units.size(); unitIndex++)
		{
			if (&theMap->m_player2Units[unitIndex] == theMap->m_selectedUnit)
			{
				if (unitIndex == 0)
				{
					for (int nextUnitIndex = static_cast<int>(theMap->m_player2Units.size() - 1); nextUnitIndex >= 0; nextUnitIndex--)
					{
						if (!theMap->m_player2Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player2Units[nextUnitIndex];
							break;
						}
					}

				}
				else
				{
					for (int nextUnitIndex = unitIndex - 1; nextUnitIndex != unitIndex; nextUnitIndex--)
					{
						if (!theMap->m_player2Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player2Units[nextUnitIndex];
							break;
						}

						if (nextUnitIndex == 0)
						{
							nextUnitIndex = static_cast<int>(theMap->m_player2Units.size());
						}
					}
				}
				break;
			}
		}
	}

	return true;
}


bool Map::Event_SelectNextUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	if (theMap->m_currentPlayerTurn == 1)
	{
		for (int unitIndex = 0; unitIndex < theMap->m_player1Units.size(); unitIndex++)
		{
			if (&theMap->m_player1Units[unitIndex] == theMap->m_selectedUnit)
			{
				if (unitIndex == theMap->m_player1Units.size() - 1)
				{
					for (int nextUnitIndex = 0; nextUnitIndex < theMap->m_player1Units.size() - 1; nextUnitIndex++)
					{
						if (!theMap->m_player1Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player1Units[nextUnitIndex];
							break;
						}
					}

				}
				else
				{
					for (int nextUnitIndex = unitIndex + 1; nextUnitIndex != unitIndex; nextUnitIndex++)
					{
						if (!theMap->m_player1Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player1Units[nextUnitIndex];
							break;
						}

						if (nextUnitIndex == theMap->m_player1Units.size() - 1)
						{
							nextUnitIndex = -1;
						}
					}
				}
				break;
			}
		}
	}
	else //if m_currentPlayerTurn == 2
	{
		for (int unitIndex = 0; unitIndex < theMap->m_player2Units.size(); unitIndex++)
		{
			if (&theMap->m_player2Units[unitIndex] == theMap->m_selectedUnit)
			{
				if (unitIndex == theMap->m_player2Units.size() - 1)
				{
					for (int nextUnitIndex = 0; nextUnitIndex < theMap->m_player2Units.size() - 1; nextUnitIndex++)
					{
						if (!theMap->m_player2Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player2Units[nextUnitIndex];
							break;
						}
					}

				}
				else
				{
					for (int nextUnitIndex = unitIndex + 1; nextUnitIndex != unitIndex; nextUnitIndex++)
					{
						if (!theMap->m_player2Units[nextUnitIndex].m_movedThisTurn)
						{
							theMap->m_selectedUnit = &theMap->m_player2Units[nextUnitIndex];
							break;
						}

						if (nextUnitIndex == theMap->m_player2Units.size() - 1)
						{
							nextUnitIndex = -1;
						}
					}
				}
				break;
			}
		}
	}

	return true;
}


bool Map::Event_MoveUnit(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	theMap->m_playerState = PlayerState::UNIT_MOVED;

	theMap->m_selectedUnit->m_coords = theMap->m_selectedTileCoords;

	return true;
}


bool Map::Event_ConfirmMove(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	if (theMap->m_selectedUnit->m_definition->m_type == UnitType::ARTILLERY)
	{
		theMap->m_playerState = PlayerState::SELECTING;
		theMap->m_selectedUnit->m_movedThisTurn = true;
		theMap->m_selectedUnit = nullptr;
	}
	else if (theMap->m_selectedUnit->m_definition->m_type == UnitType::TANK)
	{
		theMap->m_playerState = PlayerState::UNIT_MOVE_CONFIRMED;
	}

	return true;
}


bool Map::Event_NoAttack(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	return true;
}


bool Map::Event_Attack(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	Unit* enemyUnit = nullptr;
	if (theMap->m_currentPlayerTurn == 1)
	{
		enemyUnit = theMap->GetUnitAtCoords(theMap->m_selectedTileCoords, 2);
	}
	else //m_currentPlayerTurn == 2
	{
		enemyUnit = theMap->GetUnitAtCoords(theMap->m_selectedTileCoords, 1);
	}

	if (enemyUnit != nullptr)
	{
		//if enemy is within attack range, attack
		int tileIndex = theMap->GetTileIndex(enemyUnit->m_coords);
		float distFromUnit = theMap->m_distanceFieldFromSelectedUnit.m_values[tileIndex];
		UnitDefinition const* def = theMap->m_selectedUnit->m_definition;

		if (distFromUnit <= static_cast<float>(def->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(def->m_groundAttackRangeMin))
		{
			theMap->m_targetedUnit = enemyUnit;
			theMap->m_playerState = PlayerState::UNIT_ATTACKING;
		}
	}
	else
	{
		theMap->m_playerState = PlayerState::SELECTING;
		theMap->m_selectedUnit->m_movedThisTurn = true;
		theMap->m_selectedUnit = nullptr;
	}

	return true;
}


bool Map::Event_ConfirmAttack(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	if (theMap->m_targetedUnit == nullptr)
	{
		//ERROR_RECOVERABLE("Error! Attack attempted without targeted unit.");
		//return;

		if (theMap->m_currentPlayerTurn == 1)
		{
			theMap->m_targetedUnit = theMap->GetUnitAtCoords(theMap->m_selectedTileCoords, 2);
		}
		else //m_currentPlayerTurn == 2
		{
			theMap->m_targetedUnit = theMap->GetUnitAtCoords(theMap->m_selectedTileCoords, 1);
		}
	}

	//deal damage to target
	UnitDefinition const* myDef = theMap->m_selectedUnit->m_definition;
	UnitDefinition const* targetDef = theMap->m_targetedUnit->m_definition;
	int damageToTarget = 2 * myDef->m_groundAttackDamage / targetDef->m_defense;
	theMap->m_targetedUnit->m_currentHealth -= damageToTarget;

	//target deals damage to you if they are within range
	theMap->PopulateDistanceField(theMap->m_distanceFieldFromSelectedUnit, theMap->m_targetedUnit->m_coords);
	int tileIndex = theMap->GetTileIndex(theMap->m_selectedUnit->m_coords);
	float distFromUnit = theMap->m_distanceFieldFromSelectedUnit.m_values[tileIndex];

	if (distFromUnit <= static_cast<float>(targetDef->m_groundAttackRangeMax) && distFromUnit >= static_cast<float>(targetDef->m_groundAttackRangeMin))
	{
		int damageToSelf = 2 * targetDef->m_groundAttackDamage / targetDef->m_defense;
		theMap->m_selectedUnit->m_currentHealth -= damageToSelf;
	}

	//handle deaths
	if (theMap->m_selectedUnit->m_currentHealth <= 0)
	{
		theMap->KillUnit(theMap->m_selectedUnit);
	}
	if (theMap->m_targetedUnit->m_currentHealth <= 0)
	{
		theMap->KillUnit(theMap->m_targetedUnit);
	}

	//change state
	theMap->m_targetedUnit = nullptr;
	theMap->m_selectedUnit->m_movedThisTurn = true;
	theMap->m_selectedUnit = nullptr;
	theMap->m_playerState = PlayerState::SELECTING;

	return true;
}


bool Map::Event_CancelMove(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	theMap->m_playerState = PlayerState::SELECTING;
	theMap->m_selectedUnit->m_coords = theMap->m_previousUnitTileCoords;
	theMap->m_selectedUnit = nullptr;

	return true;
}


bool Map::Event_EndTurn(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	g_theGame->m_currentMap->m_playerState = PlayerState::ENDING_TURN;

	return true;
}


bool Map::Event_ConfirmEnd(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	Map* theMap = g_theGame->m_currentMap;

	if (theMap->m_currentPlayerTurn == 1)
	{
		theMap->m_currentPlayerTurn = 2;
		for (int unitIndex = 0; unitIndex < theMap->m_player1Units.size(); unitIndex++)
		{
			theMap->m_player1Units[unitIndex].m_movedThisTurn = false;
		}
	}
	else if (theMap->m_currentPlayerTurn == 2)
	{
		theMap->m_currentPlayerTurn = 1;
		for (int unitIndex = 0; unitIndex < theMap->m_player2Units.size(); unitIndex++)
		{
			theMap->m_player2Units[unitIndex].m_movedThisTurn = false;
		}
	}

	theMap->m_playerState = PlayerState::WAITING;
	theMap->m_selectedUnit = nullptr;

	return true;
}


bool Map::Event_CancelEnd(EventArgs& args)
{
	UNUSED(args);

	if (g_theGame == nullptr || g_theGame->m_currentMap == nullptr)
	{
		return true;
	}

	g_theGame->m_currentMap->m_playerState = PlayerState::SELECTING;

	return true;
}
