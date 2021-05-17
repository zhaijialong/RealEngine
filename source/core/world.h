#pragma once

#include "camera.h"
#include "gui.h"
#include "editor.h"
#include "i_visible_object.h"
#include <memory>

class World
{
public:
    World();

    Camera* GetCamera() const { return m_pCamera.get(); }
    GUI* GetGUI() const { return m_pGUI.get(); }

    void LoadScene(const std::string& file);
    void SaveScene(const std::string& file);
    void AddObject(IVisibleObject* object);

    void Tick(float delta_time);

private:
    void CreateVisibleObject(tinyxml2::XMLElement* element);

private:
    std::unique_ptr<Camera> m_pCamera;
    std::unique_ptr<GUI> m_pGUI;
    std::unique_ptr<Editor> m_pEditor;

    std::list<std::unique_ptr<IVisibleObject>> m_objects;
};