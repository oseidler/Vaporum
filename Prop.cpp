#include "Game/Prop.hpp"
#include "Game/Game.hpp"
#include "Engine/Renderer/Renderer.hpp"


//
//constructor
//
Prop::Prop(Game* owner)
	:Entity(owner)
{
}


//
//public game flow functions
//
void Prop::Update()
{
	//update orientation using angular velocity
	//m_orientation.m_yawDegrees += m_angularVelocity.m_yawDegrees * deltaSeconds;
	//m_orientation.m_pitchDegrees += m_angularVelocity.m_pitchDegrees * deltaSeconds;
	//m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
}


void Prop::Render() const
{
	g_theRenderer->SetBlendMode(BlendMode::OPAQUE);
	g_theRenderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
	g_theRenderer->BindShader(nullptr);
	g_theRenderer->BindTexture(m_texture);

	//set model constants
	g_theRenderer->SetModelConstants(GetModelMatrix(), m_color);

	g_theRenderer->DrawVertexArray((int)m_verts.size(), m_verts.data());
}
