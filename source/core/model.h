#pragma once

#include "i_visible_object.h"
#include "gfx/gfx.h"
#include <memory>

class Model : public IVisibleObject
{
public:
    virtual void Load(tinyxml2::XMLElement* element) override;
    virtual void Store(tinyxml2::XMLElement* element) override;
    virtual bool Create() override;
    virtual void Tick() override;

private:
    std::string m_file;
    std::vector<std::unique_ptr<IGfxTexture>> m_textures;
};
