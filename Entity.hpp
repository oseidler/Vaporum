#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"


//forward declarations
class Game;

class Entity
{
//public member functions
public:
	//constructor and destructor
	Entity(Game* owner);
	virtual ~Entity();

	//game flow functions
	virtual void Update() = 0;
	virtual void Render() const = 0;

	//entity utilities
	Mat44 GetModelMatrix() const;

//public member variables
public:
	Game* m_game = nullptr;

	Vec3 m_position;
	Vec3 m_velocity;
	EulerAngles m_orientation;
	EulerAngles m_angularVelocity;

	Rgba8 m_color = Rgba8(255, 255, 255);
};
