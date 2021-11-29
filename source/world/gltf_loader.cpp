#include "gltf_loader.h"
#include "static_mesh.h"
#include "mesh_material.h"
#include "core/engine.h"
#include "tinyxml2/tinyxml2.h"
#include "utils/string.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"


inline float3 str_to_float3(const std::string& str)
{
    std::vector<float> v;
    v.reserve(3);
    string_to_float_array(str, v);
    return float3(v[0], v[1], v[2]);
}

inline void GetTransform(cgltf_node* node, float4x4& matrix)
{
    float3 translation;
    float4 rotation;
    float3 scale;

    if (node->has_matrix)
    {
        float4x4 matrix = float4x4(node->matrix);
        decompose(matrix, translation, rotation, scale);
    }
    else
    {
        translation = float3(node->translation);
        rotation = float4(node->rotation);
        scale = float3(node->scale);
    }

    //right-hand to left-hand
    translation.z *= -1;
    rotation.z *= -1;
    rotation.w *= -1;

    float4x4 T = translation_matrix(translation);
    float4x4 R = rotation_matrix(rotation);
    float4x4 S = scaling_matrix(scale);

    matrix = mul(T, mul(R, S));
}

GLTFLoader::GLTFLoader(World* world, tinyxml2::XMLElement* element)
{
    m_pWorld = world;
    m_file = element->FindAttribute("file")->Value();

    float3 position = float3(0, 0, 0);
    float3 rotation = float3(0, 0, 0);
    float3 scale = float3(1, 1, 1);

    const tinyxml2::XMLAttribute* position_attr = element->FindAttribute("position");
    if (position_attr)
    {
        position = str_to_float3(position_attr->Value());
    }

    const tinyxml2::XMLAttribute* rotation_attr = element->FindAttribute("rotation");
    if (rotation_attr)
    {
        rotation = str_to_float3(rotation_attr->Value());
    }

    const tinyxml2::XMLAttribute* scale_attr = element->FindAttribute("scale");
    if (scale_attr)
    {
        scale = str_to_float3(scale_attr->Value());
    }

    float4x4 T = translation_matrix(position);
    float4x4 R = rotation_matrix(rotation_quat(rotation));
    float4x4 S = scaling_matrix(scale);
    m_mtxWorld = mul(T, mul(R, S));
}

void GLTFLoader::Load()
{
    std::string file = Engine::GetInstance()->GetAssetPath() + m_file;

    cgltf_options options = {};
    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse_file(&options, file.c_str(), &data);
    if (result != cgltf_result_success)
    {
        return;
    }

    cgltf_load_buffers(&options, data, file.c_str());

    for (cgltf_size i = 0; i < data->scenes_count; ++i)
    {
        for (cgltf_size node = 0; node < data->scenes[i].nodes_count; ++node)
        {
            LoadNode(data->scenes[i].nodes[node], m_mtxWorld);
        }
    }

    cgltf_free(data);
}

void GLTFLoader::LoadNode(cgltf_node* node, const float4x4& mtxParentToWorld)
{
    float4x4 mtxLocalToParent;
    GetTransform(node, mtxLocalToParent);

    float4x4 mtxLocalToWorld = mul(mtxParentToWorld, mtxLocalToParent);

    float3 position;
    float3 rotation;
    float3 scale;
    decompose(mtxLocalToWorld, position, rotation, scale);

    if (node->mesh)
    {
        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            StaticMesh* mesh = LoadMesh(&node->mesh->primitives[i], node->mesh->name ? node->mesh->name : "");

            mesh->SetPosition(position);
            mesh->SetRotation(rotation);
            mesh->SetScale(scale);
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i)
    {
        LoadNode(node->children[i], mtxLocalToWorld);
    }
}


inline bool TextureExists(const cgltf_texture_view& texture_view)
{
    if (texture_view.texture && texture_view.texture->image->uri)
    {
        return true;
    }
    return false;
}

Texture2D* GLTFLoader::LoadTexture(const std::string& file, bool srgb)
{
    size_t last_slash = m_file.find_last_of('/');
    std::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, last_slash + 1);

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    Texture2D* texture = pRenderer->CreateTexture2D(path + file, srgb);

    return texture;
}

MeshMaterial* GLTFLoader::LoadMaterial(cgltf_material* gltf_material)
{
    MeshMaterial* material = new MeshMaterial;
    if (gltf_material == nullptr)
    {
        return material;
    }

    RE_ASSERT(gltf_material->has_pbr_metallic_roughness);
    material->m_name = gltf_material->name != nullptr ? gltf_material->name : "";

    if (TextureExists(gltf_material->pbr_metallic_roughness.base_color_texture))
    {
        material->m_pAlbedoTexture = LoadTexture(gltf_material->pbr_metallic_roughness.base_color_texture.texture->image->uri, true);
    }

    if (TextureExists(gltf_material->pbr_metallic_roughness.metallic_roughness_texture))
    {
        material->m_pMetallicRoughnessTexture = LoadTexture(gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri, false);
    }

    if (TextureExists(gltf_material->normal_texture))
    {
        material->m_pNormalTexture = LoadTexture(gltf_material->normal_texture.texture->image->uri, false);
    }

    if (TextureExists(gltf_material->emissive_texture))
    {
        material->m_pEmissiveTexture = LoadTexture(gltf_material->emissive_texture.texture->image->uri, true);
    }

    if (TextureExists(gltf_material->occlusion_texture))
    {
        material->m_pAOTexture = LoadTexture(gltf_material->occlusion_texture.texture->image->uri, false);
    }

    material->m_albedoColor = float3(gltf_material->pbr_metallic_roughness.base_color_factor);
    material->m_emissiveColor = float3(gltf_material->emissive_factor);
    material->m_metallic = gltf_material->pbr_metallic_roughness.metallic_factor;
    material->m_roughness = gltf_material->pbr_metallic_roughness.roughness_factor;
    material->m_alphaCutoff = gltf_material->alpha_cutoff;
    material->m_bAlphaTest = gltf_material->alpha_mode == cgltf_alpha_mode_mask;

    return material;
}

IndexBuffer* LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name)
{
    RE_ASSERT(accessor->component_type == cgltf_component_type_r_16u || accessor->component_type == cgltf_component_type_r_32u);

    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t index_count = (uint32_t)accessor->count;
    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IndexBuffer* buffer = pRenderer->CreateIndexBuffer(data, stride, index_count, name);

    return buffer;
}

StructuredBuffer* LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, bool convertToLH)
{
    RE_ASSERT(accessor->component_type == cgltf_component_type_r_32f ||
        accessor->component_type == cgltf_component_type_r_16u); //bone id

    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t size = stride * (uint32_t)accessor->count;
    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    //convert right-hand to left-hand
    if (convertToLH)
    {
        for (uint32_t i = 0; i < (uint32_t)accessor->count; ++i)
        {
            float3* v = (float3*)data + i;
            v->z = -v->z;
        }
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    StructuredBuffer* buffer = pRenderer->CreateStructuredBuffer(data, stride, (uint32_t)accessor->count, name);

    return buffer;
}

StaticMesh* GLTFLoader::LoadMesh(cgltf_primitive* primitive, const std::string& name)
{
    StaticMesh* mesh = new StaticMesh(m_file + " " + name);
    mesh->m_pMaterial.reset(LoadMaterial(primitive->material));

    mesh->m_pIndexBuffer.reset(LoadIndexBuffer(primitive->indices, "model(" + m_file + " " + name + ") IB"));

    for (cgltf_size i = 0; i < primitive->attributes_count; ++i)
    {
        switch (primitive->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            mesh->m_pPosBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") pos", true));
            break;
        case cgltf_attribute_type_texcoord:
            if (primitive->attributes[i].index == 0)
            {
                mesh->m_pUVBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") UV", false));
            }
            break;
        case cgltf_attribute_type_normal:
            mesh->m_pNormalBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") normal", true));
            break;
        case cgltf_attribute_type_tangent:
            mesh->m_pTangentBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") tangent", false));
            break;
        /*
        case cgltf_attribute_type_joints:
            mesh->boneIDBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") boneID", false));
            break;
        case cgltf_attribute_type_weights:
            mesh->boneWeightBuffer.reset(LoadVertexBuffer(primitive->attributes[i].data, "model(" + m_file + " " + name + ") boneWeight", false));
            break;
        */
        default:
            break;
        }
    }

    m_pWorld->AddObject(mesh);
    return mesh;
}
