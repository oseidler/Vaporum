#include "Game/Entity.hpp"
#include "Game/Game.hpp"


//
//constructor and destructor
//
Entity::Entity(Game* owner)
	:m_game(owner)
{
}


Entity::~Entity()
{
}


//
//public entity utilities
//
Mat44 Entity::GetModelMatrix() const
{
	Mat44 modelMatrix = m_orientation.GetAsMatrix_XFwd_YLeft_ZUp();

	modelMatrix.SetTranslation3D(m_position);

	return modelMatrix;
}
