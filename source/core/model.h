#pragma once

#include "i_visible_object.h"
#include "renderer/renderer.h"

struct cgltf_data;

class Model : public IVisibleObject
{
public:
    virtual void Load(tinyxml2::XMLElement* element) override;
    virtual void Store(tinyxml2::XMLElement* element) override;
    virtual bool Create() override;
    virtual void Tick() override;

private:
    void LoadTextures(cgltf_data* data);

private:
    std::string m_file;

    std::vector<std::unique_ptr<Texture>> m_textures;
};
