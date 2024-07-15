#pragma once
#include "Game/MapDefinition.hpp"
#include "Game/Tile.hpp"
#include "Game/Unit.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/HeatMaps.hpp"


class VertexBuffer;
class IndexBuffer;
struct Vertex_PCUTBN;


enum class PlayerState
{
	READY,
	SELECTING,
	UNIT_SELECTED,
	UNIT_MOVED,
	UNIT_MOVE_CONFIRMED,
	UNIT_ATTACKING,
	ENDING_TURN,
	WAITING
};


class Map
{
//public member functions
public:
	//constructor and destructor
	Map(MapDefinition const* definition);
	Map(Map const& copy) = delete;
	~Map();

	//game flow functions
	void Update();
	void Render() const;

	//map utilities
	Vec3 PerformMouseRaycast();
	int  GetTileIndex(IntVec2 tileCoords) const;
	bool IsTileInBounds(Tile const& tile) const;
	bool IsTileSelectable(Tile const& tile) const;
	void EndTurn();
	Unit* GetUnitAtCoords(IntVec2 tileCoords, int player) const;
	void RevertOrders();
	void AttemptMove();
	void ConfirmMove();
	void AttemptAttack();
	void AttackTargetedUnit();
	void KillUnit(Unit* unit);
	void PopulateDistanceField(TileHeatMap& outDistanceField, IntVec2 const& referenceCoords);

	//network commands
	static bool Event_StartTurn(EventArgs& args);
	static bool Event_SelectHex(EventArgs& args);
	static bool Event_SelectUnit(EventArgs& args);
	static bool Event_SelectFirstUnit(EventArgs& args);
	static bool Event_SelectLastUnit(EventArgs& args);
	static bool Event_SelectPreviousUnit(EventArgs& args);
	static bool Event_SelectNextUnit(EventArgs& args);
	static bool Event_MoveUnit(EventArgs& args);
	static bool Event_ConfirmMove(EventArgs& args);
	static bool Event_NoAttack(EventArgs& args);
	static bool Event_Attack(EventArgs& args);
	static bool Event_ConfirmAttack(EventArgs& args);
	static bool Event_CancelMove(EventArgs& args);
	static bool Event_EndTurn(EventArgs& args);
	static bool Event_ConfirmEnd(EventArgs& args);
	static bool Event_CancelEnd(EventArgs& args);

//public member variables
public:
	MapDefinition const* m_definition = nullptr;

	std::vector<Tile> m_tiles;
	IntVec2 m_selectedTileCoords = IntVec2(-1, -1);
	TileHeatMap m_distanceFieldFromSelectedUnit;

	int m_currentPlayerTurn = 1;
	PlayerState m_playerState = PlayerState::READY;

	mutable std::vector<Unit> m_player1Units;
	mutable std::vector<Unit> m_player2Units;

	std::vector<Vertex_PCUTBN>  m_tileVerts;
	std::vector<unsigned int> m_tileVertIndexes;
	VertexBuffer*			  m_tileVertBuffer;
	IndexBuffer*			  m_tileIndexBuffer;

	Unit* m_selectedUnit = nullptr;
	Unit* m_targetedUnit = nullptr;

	IntVec2 m_previousUnitTileCoords = IntVec2(-1, -1);
};
