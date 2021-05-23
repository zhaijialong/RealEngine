#include "i_light.h"

void ILight::Load(tinyxml2::XMLElement* element)
{
    IVisibleObject::Load(element);
}

void ILight::Store(tinyxml2::XMLElement* element)
{
    IVisibleObject::Store(element);
}
