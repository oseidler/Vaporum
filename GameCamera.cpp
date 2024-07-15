#include "Game/GameCamera.hpp"
#include "Game/Game.hpp"
#include "Game/Map.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"


//
//constructor
//
GameCamera::GameCamera(Game* owner)
	:Entity(owner)
{
}


//
//public game flow functions
//
void GameCamera::Update()
{
	float systemDeltaSeconds = Clock::GetSystemClock().GetDeltaSeconds();

	m_isSpeedUp = false;

	Vec3 movementIntentions = Vec3();

	if (g_theInput->IsKeyDown('W'))
	{
		movementIntentions.y += 1.0f;
	}
	if (g_theInput->IsKeyDown('S'))
	{
		movementIntentions.y -= 1.0f;
	}
	if (g_theInput->IsKeyDown('A'))
	{
		movementIntentions.x -= 1.0f;
	}
	if (g_theInput->IsKeyDown('D'))
	{
		movementIntentions.x += 1.0f;
	}
	if (g_theInput->IsKeyDown('Q'))
	{
		movementIntentions.z += 1.0f;
	}
	if (g_theInput->IsKeyDown('E'))
	{
		movementIntentions.z -= 1.0f;
	}

	if (g_theInput->IsKeyDown(KEYCODE_SHIFT))
	{
		m_isSpeedUp = true;
	}

	m_velocity = Vec3(movementIntentions.x, movementIntentions.y, 0.0f) * m_panSpeed;
	m_velocity += Vec3(0.0f, 0.0f, movementIntentions.z) * m_elevateSpeed;

	UpdateFromController();

	if (m_isSpeedUp)
	{
		systemDeltaSeconds *= 10.0f;
	}

	m_position += m_velocity * systemDeltaSeconds;

	//clamp camera according to min height, then world z, then projected world xy
	m_position.z = GetClamped(m_position.z, m_minHeight, FLT_MAX);
	MapDefinition const* mapDef = g_theGame->m_currentMap->m_definition;
	m_position.z = GetClamped(m_position.z, mapDef->m_boundsMin.z, mapDef->m_boundsMax.z);
	Vec3 raycastVector = GetModelMatrix().GetIBasis3D() * FLT_MAX;
	float t = (-m_position.z) / raycastVector.z;
	Vec3 posAtT = m_position + (raycastVector * t);
	
	float clampedX = GetClamped(posAtT.x, g_theGame->m_currentMap->m_definition->m_boundsMin.x, g_theGame->m_currentMap->m_definition->m_boundsMax.x);
	float clampedY = GetClamped(posAtT.y, g_theGame->m_currentMap->m_definition->m_boundsMin.y, g_theGame->m_currentMap->m_definition->m_boundsMax.y);
	float offsetX = posAtT.x - clampedX;
	m_position.x -= offsetX;
	float offsetY = posAtT.y - clampedY;
	m_position.y -= offsetY;

	m_camera.SetTransform(m_position, m_orientation);
}


void GameCamera::UpdateFromController()
{
	XboxController const& controller = g_theInput->GetController(0);
	AnalogJoystick const& leftStick = controller.GetLeftStick();
	float leftStickMagnitude = leftStick.GetMagnitude();

	Vec3 movementIntentions = Vec3();

	if (leftStickMagnitude > 0.0f)
	{
		movementIntentions.y = -leftStick.GetPosition().x;
		movementIntentions.x = leftStick.GetPosition().y;

		m_velocity = Vec3(movementIntentions.x, movementIntentions.y, 0.0f) * m_panSpeed;
	}

	if (controller.IsButtonDown(XBOX_BUTTON_L))
	{
		movementIntentions.z += 1.0f;
		m_velocity += Vec3(0.0f, 0.0f, movementIntentions.z) * m_panSpeed;
	}
	if (controller.IsButtonDown(XBOX_BUTTON_R))
	{
		movementIntentions.z -= 1.0f;
		m_velocity += Vec3(0.0f, 0.0f, movementIntentions.z) * m_panSpeed;
	}

	if (controller.IsButtonDown(XBOX_BUTTON_A))
	{
		m_isSpeedUp = true;
	}
}


void GameCamera::Render() const
{
}
