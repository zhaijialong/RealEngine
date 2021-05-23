#include "i_visible_object.h"
#include "utils/string.h"

void IVisibleObject::Load(tinyxml2::XMLElement* element)
{
    const tinyxml2::XMLAttribute* position = element->FindAttribute("position");
    if (position)
    {
        std::vector<float> v;
        string_to_float_array(position->Value(), v);
        m_pos = vector_to_float3(v);
    }

    const tinyxml2::XMLAttribute* rotation = element->FindAttribute("rotation");
    if (rotation)
    {
        std::vector<float> v;
        string_to_float_array(rotation->Value(), v);
        m_rotation = vector_to_float3(v);
    }

    const tinyxml2::XMLAttribute* scale = element->FindAttribute("scale");
    if (scale)
    {
        std::vector<float> v;
        string_to_float_array(scale->Value(), v);
        m_scale = vector_to_float3(v);
    }
}

void IVisibleObject::Store(tinyxml2::XMLElement* element)
{
}
