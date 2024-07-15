#include "Game/Tile.hpp"
#include "Engine/Core/Vertex_PCUTBN.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/VertexUtils.hpp"


//query functions
float Tile::GetCenterPosX() const
{
	float centerXPosInHexSpace = static_cast<float>(m_coords.x);
	return 0.866f * centerXPosInHexSpace;
}


float Tile::GetCenterPosY() const
{
	float centerXPosInHexSpace = static_cast<float>(m_coords.x);
	float centerYPosInHexSpace = static_cast<float>(m_coords.y);
	return 0.5f * centerXPosInHexSpace + centerYPosInHexSpace;
}


Vec3 Tile::GetCenterPos() const
{
	float centerXPosInHexSpace = static_cast<float>(m_coords.x);
	float centerYPosInHexSpace = static_cast<float>(m_coords.y);

	float centerXPos = 0.866f * centerXPosInHexSpace;
	float centerYPos = 0.5f * centerXPosInHexSpace + centerYPosInHexSpace;

	return Vec3(centerXPos, centerYPos, 0.0f);
}


bool Tile::IsPointInsideTile(Vec3 const& point) const
{
	std::vector<Vec3> hexVertexes;

	//get all six vertexes
	Vec3 centerPos = GetCenterPos();
	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float degrees = static_cast<float>(edgeIndex) * 60.0f;
		float cos = CosDegrees(degrees);
		float sin = SinDegrees(degrees);

		hexVertexes.emplace_back(centerPos.x + CIRCUMRADIUS * cos, centerPos.y + CIRCUMRADIUS * sin, 0.0f);
	}

	//do the Is Point In Convex Polygon test for each vertex
	for (int vertIndex = 0; vertIndex < 6; vertIndex++)
	{
		int nextVertIndex = vertIndex + 1;
		if (nextVertIndex == 6) nextVertIndex = 0;
		Vec3 edgeVector = hexVertexes[nextVertIndex] - hexVertexes[vertIndex];

		Vec3 toPoint = point - hexVertexes[vertIndex];
		Vec3 normal = CrossProduct3D(edgeVector, toPoint);
		if (normal.z < 0.0f)
		{
			return false;
		}
	}

	//if we get here, all cross products were positive, so point is inside
	return true;
}


int Tile::GetTaxicabDistance(IntVec2 const& otherTile) const
{
	return (abs(m_coords.x - otherTile.x) + abs(m_coords.x + m_coords.y - otherTile.x - otherTile.y) + abs(m_coords.y - otherTile.y)) / 2;
}


//
//vertex adding functions
//
void Tile::AddVertsForTile(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indexes) const
{
	float innerCR = CIRCUMRADIUS - TILE_EDGE_WIDTH * 0.5f;
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f;

	Vec3 centerPos = GetCenterPos();

	int vertexBufferSize = static_cast<int>(verts.size());

	for (int vertIndex = 0; vertIndex < 6; vertIndex++)
	{
		float degrees = static_cast<float>(vertIndex) * 60.0f;
		float cos = CosDegrees(degrees);
		float sin = SinDegrees(degrees);

		Vec3 innerPosition = Vec3(centerPos.x + innerCR * cos, centerPos.y + innerCR * sin, 0.0f);
		Vec3 outerPosition = Vec3(centerPos.x + outerCR * cos, centerPos.y + outerCR * sin, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(innerPosition, Vec3(0.0f, 0.0f, 1.0f), Rgba8()));
		verts.emplace_back(Vertex_PCUTBN(outerPosition, Vec3(0.0f, 0.0f, 1.0f), Rgba8()));
	}

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		int indexStart = edgeIndex * 2;
		if (indexStart >= 12) indexStart -= 12;
		int firstVertex = indexStart + 2;
		if (firstVertex >= 12) firstVertex -= 12;
		int secondVertex = indexStart;
		int thirdVertex = indexStart + 1;
		int fourthVertex = indexStart + 3;
		if (fourthVertex >= 12) fourthVertex -= 12;

		indexes.emplace_back(firstVertex + vertexBufferSize);
		indexes.emplace_back(secondVertex + vertexBufferSize);
		indexes.emplace_back(thirdVertex + vertexBufferSize);
		indexes.emplace_back(firstVertex + vertexBufferSize);
		indexes.emplace_back(thirdVertex + vertexBufferSize);
		indexes.emplace_back(fourthVertex + vertexBufferSize);
	}
}


void Tile::AddVertsForSelectedTile(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;
	float innerCR = outerCR - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);
		Vec3 innerStartPosition = Vec3(centerPos.x + innerCR * cosStart, centerPos.y + innerCR * sinStart, 0.0f);
		Vec3 innerEndPosition = Vec3(centerPos.x + innerCR * cosEnd, centerPos.y + innerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(innerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_TILE_COLOR));
	}
}


void Tile::AddVertsForSelectedUnit(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;
	float innerCR = outerCR - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);
		Vec3 innerStartPosition = Vec3(centerPos.x + innerCR * cosStart, centerPos.y + innerCR * sinStart, 0.0f);
		Vec3 innerEndPosition = Vec3(centerPos.x + innerCR * cosEnd, centerPos.y + innerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));
		verts.emplace_back(Vertex_PCUTBN(innerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), SELECTED_UNIT_COLOR));
	}
}


void Tile::AddVertsForTileInMoveRange(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(centerPos, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_RANGE_COLOR));
	}
}


void Tile::AddVertsForTileOnMovePath(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(centerPos, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_PATH_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_PATH_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), MOVEMENT_PATH_COLOR));
	}
}


void Tile::AddVertsForTileInAttackRange(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;
	float innerCR = outerCR - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);
		Vec3 innerStartPosition = Vec3(centerPos.x + innerCR * cosStart, centerPos.y + innerCR * sinStart, 0.0f);
		Vec3 innerEndPosition = Vec3(centerPos.x + innerCR * cosEnd, centerPos.y + innerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(innerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_RANGE_COLOR));
	}
}


void Tile::AddVertsForTileBeingAttacked(std::vector<Vertex_PCUTBN>& verts) const
{
	float outerCR = CIRCUMRADIUS + TILE_EDGE_WIDTH * 0.5f - 0.1f;
	float innerCR = outerCR - 0.1f;

	Vec3 centerPos = GetCenterPos();

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * 60.0f;
		float endDegrees = static_cast<float>(edgeIndex + 1) * 60.0f;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec3 outerStartPosition = Vec3(centerPos.x + outerCR * cosStart, centerPos.y + outerCR * sinStart, 0.0f);
		Vec3 outerEndPosition = Vec3(centerPos.x + outerCR * cosEnd, centerPos.y + outerCR * sinEnd, 0.0f);
		Vec3 innerStartPosition = Vec3(centerPos.x + innerCR * cosStart, centerPos.y + innerCR * sinStart, 0.0f);
		Vec3 innerEndPosition = Vec3(centerPos.x + innerCR * cosEnd, centerPos.y + innerCR * sinEnd, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(innerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));

		verts.emplace_back(Vertex_PCUTBN(innerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerStartPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));
		verts.emplace_back(Vertex_PCUTBN(outerEndPosition, Vec3(0.0f, 0.0f, 1.0f), ATTACKING_TILE_COLOR));
	}
}


void Tile::AddVertsForBlockedTile(std::vector<Vertex_PCUTBN>& verts, std::vector<unsigned int>& indexes) const
{
	float innerCR = CIRCUMRADIUS - TILE_EDGE_WIDTH * 0.5f;

	Vec3 centerPos = GetCenterPos();

	int vertexBufferSize = static_cast<int>(verts.size());

	verts.emplace_back(Vertex_PCUTBN(centerPos, Vec3(0.0f, 0.0f, 1.0f), BLOCKED_TILE_COLOR));

	for (int vertIndex = 0; vertIndex < 6; vertIndex++)
	{
		float degrees = static_cast<float>(vertIndex) * 60.0f;
		float cos = CosDegrees(degrees);
		float sin = SinDegrees(degrees);

		Vec3 outerPosition = Vec3(centerPos.x + innerCR * cos, centerPos.y + innerCR * sin, 0.0f);

		verts.emplace_back(Vertex_PCUTBN(outerPosition, Vec3(0.0f, 0.0f, 1.0f), BLOCKED_TILE_COLOR));
	}

	for (int edgeIndex = 0; edgeIndex < 6; edgeIndex++)
	{
		/*int indexStart = edgeIndex * 2;
		if (indexStart >= 12) indexStart -= 12;
		int firstVertex = indexStart + 2;
		if (firstVertex >= 12) firstVertex -= 12;
		int secondVertex = indexStart;
		int thirdVertex = indexStart + 1;
		int fourthVertex = indexStart + 3;
		if (fourthVertex >= 12) fourthVertex -= 12;

		indexes.emplace_back(firstVertex + vertexBufferSize);
		indexes.emplace_back(secondVertex + vertexBufferSize);
		indexes.emplace_back(thirdVertex + vertexBufferSize);
		indexes.emplace_back(firstVertex + vertexBufferSize);
		indexes.emplace_back(thirdVertex + vertexBufferSize);
		indexes.emplace_back(fourthVertex + vertexBufferSize);*/

		indexes.emplace_back(vertexBufferSize);
		indexes.emplace_back(vertexBufferSize + edgeIndex + 1);
		if (edgeIndex + 2 >= 7)
		{
			indexes.emplace_back(vertexBufferSize + 1);
		}
		else
		{
			indexes.emplace_back(vertexBufferSize + edgeIndex + 2);
		}
	}
}
