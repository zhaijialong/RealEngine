#pragma once

#include "camera.h"
#include "gui.h"
#include <memory>

class World
{
public:
    World();

    Camera* GetCamera() const { return m_pCamera.get(); }
    GUI* GetGUI() const { return m_pGUI.get(); }

    void Tick(float delta_time);

private:
    void TickGUI();

private:
    std::unique_ptr<Camera> m_pCamera;
    std::unique_ptr<GUI> m_pGUI;
};