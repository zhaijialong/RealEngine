#include "world.h"
#include "engine.h"
#include "model.h"
#include "sky_sphere.h"
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

    m_objects.clear();

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

void World::Tick(float delta_time)
{
    m_pGUI->Tick();
    m_pEditor->Tick();
    m_pCamera->Tick(delta_time);

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
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

void World::CreateVisibleObject(tinyxml2::XMLElement* element)
{
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
