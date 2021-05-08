#include "model.h"

void Model::Load(tinyxml2::XMLElement* element)
{
    m_file = element->FindAttribute("file")->Value();
}

void Model::Store(tinyxml2::XMLElement* element)
{
}

bool Model::Create()
{
    return true;
}

void Model::Tick()
{

}