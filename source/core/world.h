#pragma once

#include "camera.h"
#include <memory>

class World
{
public:
    World();

    Camera* GetCamera() const { return m_pCamera.get(); }

private:
    std::unique_ptr<Camera> m_pCamera;
};