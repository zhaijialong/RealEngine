#include "world.h"

World::World()
{
    m_pCamera = std::make_unique<Camera>();
}
