#pragma once

#include "tinyxml2/tinyxml2.h"

class IVisibleObject
{
public:
    virtual ~IVisibleObject() {}

    virtual void Load(tinyxml2::XMLElement* element) = 0;
    virtual void Store(tinyxml2::XMLElement* element) = 0;

    virtual bool Create() = 0;
    virtual void Tick() = 0;
};