#pragma once

#include "camera.h"
#include "i_light.h"
#include <memory>

namespace tinyxml2
{
    class XMLElement;
}

class World
{
public:
    World();

    Camera* GetCamera() const { return m_pCamera.get(); }

    void LoadScene(const std::string& file);
    void SaveScene(const std::string& file);

    void AddObject(IVisibleObject* object);
    void AddLight(ILight* light);

    void Tick(float delta_time);

    IVisibleObject* GetSelectedObject() const;
    ILight* GetPrimaryLight() const;

private:
    void ClearScene();

    void CreateVisibleObject(tinyxml2::XMLElement* element);
    void CreateLight(tinyxml2::XMLElement* element);
    void CreateCamera(tinyxml2::XMLElement* element);
    void CreateModel(tinyxml2::XMLElement* element);
    void CreateSky(tinyxml2::XMLElement* element);

private:
    std::unique_ptr<Camera> m_pCamera;

    std::list<std::unique_ptr<IVisibleObject>> m_objects;
    std::list<std::unique_ptr<ILight>> m_lights;

    ILight* m_pPrimaryLight = nullptr;
};