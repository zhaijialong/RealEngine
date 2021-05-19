#pragma once

#include "renderer/renderer.h"
#include "utils/math.h"
#include "tinyxml2/tinyxml2.h"

class IVisibleObject
{
public:
    virtual ~IVisibleObject() {}

    virtual void Load(tinyxml2::XMLElement* element) = 0;
    virtual void Store(tinyxml2::XMLElement* element) = 0;

    virtual bool Create() = 0;
    virtual void Tick(float delta_time) = 0;
    virtual void Render(Renderer* pRenderer) = 0;
};