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
    LoadTextures(data);

    cgltf_free(data);
    return true;
}

void Model::Tick()
{

}

void Model::LoadTextures(cgltf_data* data)
{
    size_t last_slash = m_file.find_last_of('/');
    std::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, last_slash + 1);

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    for (cgltf_size i = 0; i < data->textures_count; ++i)
    {
        std::string file = path + data->textures[i].image->uri;
        Texture* texture = pRenderer->LoadTexture(file);

        if (texture != nullptr)
        {
            m_textures.push_back(std::unique_ptr<Texture>(texture));
        }
    }
}
