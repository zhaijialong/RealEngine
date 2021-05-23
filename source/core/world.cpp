#include "world.h"
#include "engine.h"
#include "model.h"
#include "sky_sphere.h"
#include "directional_light.h"
#include "utils/assert.h"

World::World()
{
    m_pGUI = std::make_unique<GUI>();
    m_pGUI->Init();

    m_pCamera = std::make_unique<Camera>();
    m_pEditor = std::make_unique<Editor>();
}

void World::LoadScene(const std::string& file)
{    
    tinyxml2::XMLDocument doc;
    if (tinyxml2::XML_SUCCESS != doc.LoadFile(file.c_str()))
    {
        return;
    }

    ClearScene();

    tinyxml2::XMLNode* root_node = doc.FirstChild();
    RE_ASSERT(root_node != nullptr && strcmp(root_node->Value(), "scene") == 0);

    for (tinyxml2::XMLElement* element = root_node->FirstChildElement(); element != nullptr; element = (tinyxml2::XMLElement*)element->NextSibling())
    {
        CreateVisibleObject(element);
    }
}

void World::SaveScene(const std::string& file)
{
}

void World::AddObject(IVisibleObject* object)
{
    RE_ASSERT(object != nullptr);
    m_objects.push_back(std::unique_ptr<IVisibleObject>(object));
}

void World::AddLight(ILight* light)
{
    RE_ASSERT(light != nullptr);
    m_lights.push_back(std::unique_ptr<ILight>(light));
}

void World::Tick(float delta_time)
{
    m_pGUI->Tick();
    m_pEditor->Tick();
    m_pCamera->Tick(delta_time);

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        (*iter)->Tick(delta_time);
    }

    for (auto iter = m_lights.begin(); iter != m_lights.end(); ++iter)
    {
        (*iter)->Tick(delta_time);
    }

    //todo : culling, ...

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        (*iter)->Render(pRenderer);
    }
}

IVisibleObject* World::GetSelectedObject() const
{
    //todo : implements mouse pick

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        if (dynamic_cast<Model*>(iter->get()) != nullptr)
        {
            return iter->get();
        }
    }

    return nullptr;
}

ILight* World::GetPrimaryLight() const
{
    RE_ASSERT(m_pPrimaryLight != nullptr);
    return m_pPrimaryLight;
}

void World::ClearScene()
{
    m_objects.clear();
    m_lights.clear();
    m_pPrimaryLight = nullptr;
}

void World::CreateVisibleObject(tinyxml2::XMLElement* element)
{
    if (strcmp(element->Value(), "light") == 0)
    {
        CreateLight(element);
        return;
    }

    IVisibleObject* object = nullptr;

    if (strcmp(element->Value(), "model") == 0)
    {
        object = new Model();
    }
    else if(strcmp(element->Value(), "skysphere") == 0)
    {
        object = new SkySphere();
    }

    object->Load(element);

    if (!object->Create())
    {
        delete object;
        return;
    }

    AddObject(object);
}

void World::CreateLight(tinyxml2::XMLElement* element)
{
    ILight* light = nullptr;

    const tinyxml2::XMLAttribute* type = element->FindAttribute("type");
    RE_ASSERT(type != nullptr);

    if (strcmp(type->Value(), "directional") == 0)
    {
        light = new DirectionalLight();
    }
    else
    {
        //todo
        RE_ASSERT(false);
    }

    light->Load(element);

    if (!light->Create())
    {
        delete light;
        return;
    }

    AddLight(light);

    const tinyxml2::XMLAttribute* primary = element->FindAttribute("primary");
    if (primary && primary->BoolValue())
    {
        m_pPrimaryLight = light;
    }
}
