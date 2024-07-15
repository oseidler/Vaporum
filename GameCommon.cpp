#include "Game/GameCommon.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"


//global variables
RandomNumberGenerator g_rng;


//
//debug drawing functions
//
void DebugDrawLine(Vec2 const& startPosition, Vec2 const& endPosition, float width, Rgba8 const& color)
{
	//get necessary vectors
	Vec2 displacement = endPosition - startPosition;
	Vec2 forward = displacement.GetNormalized();
	Vec2 forwardStep = forward * width;
	Vec2 leftStep = forwardStep.GetRotated90Degrees();

	//get points
	Vec2 startLeft = startPosition - forwardStep + leftStep;
	Vec2 startRight = startPosition - forwardStep - leftStep;
	Vec2 endLeft = endPosition + forwardStep + leftStep;
	Vec2 endRight = endPosition + forwardStep - leftStep;

	Vertex_PCU verts[] =
	{
		Vertex_PCU(endRight, color),
		Vertex_PCU(startLeft, color),
		Vertex_PCU(startRight, color),

		Vertex_PCU(endRight, color),
		Vertex_PCU(endLeft, color),
		Vertex_PCU(startLeft, color),
	};
	
	constexpr int NUM_VERTS = sizeof(verts) / sizeof(Vertex_PCU);

	//call renderer to draw line
	g_theRenderer->DrawVertexArray(NUM_VERTS, verts);
}


void DebugDrawRing(Vec2 const& center, float radius, float width, Rgba8 const& color)
{
	float innerRadius = radius - (width * 0.5f);
	float outerRadius = radius + (width * 0.5f);

	constexpr int NUM_EDGES = 64;
	constexpr int NUM_TRIANGLES = NUM_EDGES * 2;
	constexpr int NUM_VERTEXES = NUM_TRIANGLES * 3;
	constexpr float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_EDGES);

	Vertex_PCU verts[NUM_VERTEXES];

	//make two triangles for every side
	for (int edgeIndex = 0; edgeIndex < NUM_EDGES; edgeIndex++)
	{
		float startDegrees = static_cast<float>(edgeIndex) * DEGREES_PER_SIDE;
		float endDegrees = static_cast<float>(edgeIndex + 1) * DEGREES_PER_SIDE;
		float cosStart = CosDegrees(startDegrees);
		float cosEnd = CosDegrees(endDegrees);
		float sinStart = SinDegrees(startDegrees);
		float sinEnd = SinDegrees(endDegrees);

		Vec2 innerStartPosition(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart);
		Vec2 innerEndPosition(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd);
		Vec2 outerStartPosition(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart);
		Vec2 outerEndPosition(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd);

		int vertA = 6 * edgeIndex;
		int vertB = 6 * edgeIndex + 1;
		int vertC = 6 * edgeIndex + 2;
		int vertD = 6 * edgeIndex + 3;
		int vertE = 6 * edgeIndex + 4;
		int vertF = 6 * edgeIndex + 5;

		verts[vertA] = Vertex_PCU(innerEndPosition, color);
		verts[vertB] = Vertex_PCU(innerStartPosition, color);
		verts[vertC] = Vertex_PCU(outerStartPosition, color);

		verts[vertD] = Vertex_PCU(innerEndPosition, color);
		verts[vertE] = Vertex_PCU(outerStartPosition, color);
		verts[vertF] = Vertex_PCU(outerEndPosition, color);
	}

	g_theRenderer->DrawVertexArray(NUM_VERTEXES, verts);
}
