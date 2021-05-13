#include "world.h"
#include "model.h"
#include "utils/assert.h"

World::World()
{
    m_pCamera = std::make_unique<Camera>();
    m_pGUI = std::make_unique<GUI>();
    m_pGUI->Init();
}

void World::LoadScene(const std::string& file)
{
    using namespace tinyxml2;
    
    XMLDocument doc;
    if (XML_SUCCESS != doc.LoadFile(file.c_str()))
    {
        return;
    }

    m_objects.clear();

    XMLNode* root_node = doc.FirstChild();
    RE_ASSERT(root_node != nullptr && strcmp(root_node->Value(), "scene") == 0);

    for (XMLElement* element = root_node->FirstChildElement(); element != nullptr; element = (XMLElement*)element->NextSibling())
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
    m_pCamera->Tick(delta_time);

    for (auto iter = m_objects.begin(); iter != m_objects.end(); ++iter)
    {
        (*iter)->Tick();
    }

    //todo : culling, ...
}

void World::CreateVisibleObject(tinyxml2::XMLElement* element)
{
    IVisibleObject* object = nullptr;

    if (strcmp(element->Value(), "model") == 0)
    {
        object = new Model();
    }
    else
    {
        //todo
    }

    object->Load(element);

    if (!object->Create())
    {
        delete object;
        return;
    }

    AddObject(object);
}
