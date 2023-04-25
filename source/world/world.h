#pragma once

#include "camera.h"
#include "physics/physics_system.h"
#include "light.h"

namespace tinyxml2
{
    class XMLElement;
}

class World
{
public:
    World();
    ~World();

    Camera* GetCamera() const { return m_pCamera.get(); }
    PhysicsSystem* GetPhysicsSystem() const { return m_pPhysicsSystem.get(); }

    void LoadScene(const eastl::string& file);
    void SaveScene(const eastl::string& file);

    void AddObject(IVisibleObject* object);
    void AddLight(ILight* light);

    void Tick(float delta_time);

    IVisibleObject* GetVisibleObject(uint32_t index) const;
    ILight* GetPrimaryLight() const;

private:
    void ClearScene();

    void CreateVisibleObject(tinyxml2::XMLElement* element);
    void CreateLight(tinyxml2::XMLElement* element);
    void CreateCamera(tinyxml2::XMLElement* element);
    void CreateModel(tinyxml2::XMLElement* element);
    void CreateSky(tinyxml2::XMLElement* element);

private:
    eastl::unique_ptr<Camera> m_pCamera;
    eastl::unique_ptr<PhysicsSystem> m_pPhysicsSystem;

    eastl::vector<eastl::unique_ptr<IVisibleObject>> m_objects;
    eastl::vector<eastl::unique_ptr<ILight>> m_lights;

    ILight* m_pPrimaryLight = nullptr;
};