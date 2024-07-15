#pragma once
#include "Game/Entity.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Core/EngineCommon.hpp"


//forward declarations
class Texture;

class Prop : public Entity
{
//public member functions
public:
	//constructor and destructor
	Prop(Game* owner);

	//game flow functions
	virtual void Update() override;
	virtual void Render() const override;

//public member variables
public:
	std::vector<Vertex_PCU> m_verts;
	Texture* m_texture = nullptr;
};
