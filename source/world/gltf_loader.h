#pragma once

class World;

namespace tinyxml2 
{
    class XMLElement;
}

class GLTFLoader
{
public:
    GLTFLoader(World* world, tinyxml2::XMLElement* element);

    void Load();
};