#pragma once
#include "Game/Entity.hpp"
#include "Engine/Renderer/Camera.hpp"


class GameCamera : public Entity
{
//public member functions
public:
	//constructor and destructor
	GameCamera(Game* owner);

	//game flow functions
	virtual void Update() override;
	void UpdateFromController();
	virtual void Render() const override;

//public member variables
public:
	Camera m_camera;

	float m_panSpeed = 1.0f;
	float m_elevateSpeed = 0.5f;
	float m_minHeight = 0.0f;
	float m_controllerTurnRate = 120.0f;
	bool m_isSpeedUp = false;
};
