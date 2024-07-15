#pragma once
#include "Game/TileDefinition.hpp"
#include "Engine/Math/IntVec2.hpp"


struct Vertex_PCUTBN;
struct Vec3;


constexpr float TILE_EDGE_WIDTH = 0.075f;
//constexpr float BLOCKED_TILE_EDGE_WIDTH = 0.05f;
constexpr float CIRCUMRADIUS = 0.57735f;
constexpr float INRADIUS = 0.5f;

static Rgba8 const SELECTED_TILE_COLOR = Rgba8(0, 255, 0);
static Rgba8 const SELECTED_UNIT_COLOR = Rgba8(0, 0, 255);
static Rgba8 const ATTACKING_RANGE_COLOR = Rgba8(125, 0, 0);
static Rgba8 const ATTACKING_TILE_COLOR = Rgba8(255, 5, 5);
static Rgba8 const MOVEMENT_RANGE_COLOR = Rgba8(255, 255, 255, 100);
static Rgba8 const MOVEMENT_PATH_COLOR = Rgba8(255, 255, 255, 200);
static Rgba8 const BLOCKED_TILE_COLOR = Rgba8(0, 0, 0);


class Tile
{
//public member variables
public:
	IntVec2 m_coords = IntVec2(-1, -1);

	TileDefinition const* m_definition = nullptr;

//public member functions
public:
	//constructor
	explicit Tile(TileDefinition const* definition, IntVec2 coords)
		: m_definition(definition)
		, m_coords(coords)
	{}

	//query functions
	float GetCenterPosX() const;
	float GetCenterPosY() const;
	Vec3  GetCenterPos() const;
	bool  IsPointInsideTile(Vec3 const& point) const;
	int   GetTaxicabDistance(IntVec2 const& otherTile) const;

	//vertex adding functions
	void AddVertsForTile(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indexes) const;
	void AddVertsForSelectedTile(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForSelectedUnit(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForTileInMoveRange(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForTileOnMovePath(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForTileInAttackRange(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForTileBeingAttacked(std::vector<Vertex_PCUTBN>& verts) const;
	void AddVertsForBlockedTile(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indexes) const;
};
