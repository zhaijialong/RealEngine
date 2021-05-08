#include "model.h"
#include "engine.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

void Model::Load(tinyxml2::XMLElement* element)
{
    m_file = element->FindAttribute("file")->Value();
}

void Model::Store(tinyxml2::XMLElement* element)
{
}

bool Model::Create()
{
    std::string file = Engine::GetInstance()->GetAssetPath() + m_file;

    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, file.c_str(), &data);
    if (result != cgltf_result_success)
    {
        return false;
    }

    cgltf_load_buffers(&options, data, file.c_str());


    cgltf_free(data);
    return true;
}

void Model::Tick()
{

}
