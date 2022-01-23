#include "gltf_loader.h"
#include "static_mesh.h"
#include "mesh_material.h"
#include "resource_cache.h"
#include "core/engine.h"
#include "utils/string.h"
#include "tinyxml2/tinyxml2.h"
#include "meshoptimizer/meshoptimizer.h"

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

inline uint32_t GetMeshIndex(cgltf_data* data, cgltf_mesh* mesh)
{
    for (cgltf_size i = 0; i < data->meshes_count; ++i)
    {
        if (&data->meshes[i] == mesh)
        {
            return (uint32_t)i;
        }
    }

    RE_ASSERT(false);
    return 0;
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
            LoadNode(data, data->scenes[i].nodes[node], m_mtxWorld);
        }
    }

    cgltf_free(data);
}

void GLTFLoader::LoadNode(cgltf_data* data, cgltf_node* node, const float4x4& mtxParentToWorld)
{
    float4x4 mtxLocalToParent;
    GetTransform(node, mtxLocalToParent);

    float4x4 mtxLocalToWorld = mul(mtxParentToWorld, mtxLocalToParent);

    //gltf 2.0 spec : If the determinant is a positive value, the winding order triangle faces is counterclockwise; in the opposite case, the winding order is clockwise.
    bool bFrontFaceCCW = determinant(mtxLocalToWorld) > 0.0f;

    float3 position;
    float3 rotation;
    float3 scale;
    decompose(mtxLocalToWorld, position, rotation, scale);

    if (node->mesh)
    {
        uint32_t mesh_index = GetMeshIndex(data, node->mesh);

        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            std::string name = "mesh_" + std::to_string(mesh_index) + "_" + std::to_string(i) + " " + node->mesh->name;

            StaticMesh* mesh = LoadMesh(&node->mesh->primitives[i], name);

            mesh->m_pMaterial->m_bFrontFaceCCW = bFrontFaceCCW;
            mesh->SetPosition(position);
            mesh->SetRotation(rotation);
            mesh->SetScale(scale);
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i)
    {
        LoadNode(data, node->children[i], mtxLocalToWorld);
    }
}

Texture2D* GLTFLoader::LoadTexture(const cgltf_texture_view& texture_view, bool srgb)
{
    if (texture_view.texture == nullptr || texture_view.texture->image->uri == nullptr)
    {
        return nullptr;
    }

    size_t last_slash = m_file.find_last_of('/');
    std::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, last_slash + 1);

    Texture2D* texture = ResourceCache::GetInstance()->GetTexture2D(path + texture_view.texture->image->uri, srgb);

    return texture;
}

MeshMaterial* GLTFLoader::LoadMaterial(cgltf_material* gltf_material)
{
    MeshMaterial* material = new MeshMaterial;
    if (gltf_material == nullptr)
    {
        return material;
    }

    material->m_name = gltf_material->name != nullptr ? gltf_material->name : "";

    if (gltf_material->has_pbr_metallic_roughness)
    {
        material->m_bPbrMetallicRoughness = true;
        material->m_pAlbedoTexture = LoadTexture(gltf_material->pbr_metallic_roughness.base_color_texture, true);
        material->m_pMetallicRoughnessTexture = LoadTexture(gltf_material->pbr_metallic_roughness.metallic_roughness_texture, false);
        material->m_albedoColor = float3(gltf_material->pbr_metallic_roughness.base_color_factor);
        material->m_metallic = gltf_material->pbr_metallic_roughness.metallic_factor;
        material->m_roughness = gltf_material->pbr_metallic_roughness.roughness_factor;
    }
    else if (gltf_material->has_pbr_specular_glossiness)
    {
        material->m_bPbrSpecularGlossiness = true;
        material->m_pDiffuseTexture = LoadTexture(gltf_material->pbr_specular_glossiness.diffuse_texture, true);
        material->m_pSpecularGlossinessTexture = LoadTexture(gltf_material->pbr_specular_glossiness.specular_glossiness_texture, true);
        material->m_diffuseColor = float3(gltf_material->pbr_specular_glossiness.diffuse_factor);
        material->m_specularColor = float3(gltf_material->pbr_specular_glossiness.specular_factor);
        material->m_glossiness = gltf_material->pbr_specular_glossiness.glossiness_factor;
    }

    material->m_pNormalTexture = LoadTexture(gltf_material->normal_texture, false);
    material->m_pEmissiveTexture = LoadTexture(gltf_material->emissive_texture, true);
    material->m_pAOTexture = LoadTexture(gltf_material->occlusion_texture, false);

    material->m_emissiveColor = float3(gltf_material->emissive_factor);
    material->m_alphaCutoff = gltf_material->alpha_cutoff;
    material->m_bAlphaTest = gltf_material->alpha_mode == cgltf_alpha_mode_mask;
    material->m_bDoubleSided = gltf_material->double_sided;

    return material;
}

meshopt_Stream LoadBufferStream(const cgltf_accessor* accessor, bool convertToLH, size_t& count)
{
    uint32_t stride = (uint32_t)accessor->stride;

    meshopt_Stream stream;
    stream.data = RE_ALLOC(stride * accessor->count);
    stream.size = stride;
    stream.stride = stride;

    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
    memcpy((void*)stream.data, data, stride * accessor->count);

    //convert right-hand to left-hand
    if (convertToLH)
    {
        RE_ASSERT(stride >= sizeof(float3));

        for (uint32_t i = 0; i < (uint32_t)accessor->count; ++i)
        {
            float3* v = (float3*)stream.data + i;
            v->z = -v->z;
        }
    }


    count = accessor->count;

    return stream;
}

StaticMesh* GLTFLoader::LoadMesh(cgltf_primitive* primitive, const std::string& name)
{
    StaticMesh* mesh = new StaticMesh(m_file + " " + name);
    mesh->m_pMaterial.reset(LoadMaterial(primitive->material));

    size_t index_count;
    meshopt_Stream indices = LoadBufferStream(primitive->indices, false, index_count);

    size_t vertex_count;
    std::vector<meshopt_Stream> vertex_streams;
    std::vector<cgltf_attribute_type> vertex_types;

    for (cgltf_size i = 0; i < primitive->attributes_count; ++i)
    {
        switch (primitive->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            vertex_streams.push_back(LoadBufferStream(primitive->attributes[i].data, true, vertex_count));
            vertex_types.push_back(primitive->attributes[i].type);
            {
                float3 min = float3(primitive->attributes[i].data->min);
                min.z = -min.z;

                float3 max = float3(primitive->attributes[i].data->max);
                max.z = -max.z;

                float3 center = (min + max) / 2;
                float radius = length(max - min) / 2;

                mesh->m_center = center;
                mesh->m_radius = radius;
            }
            break;
        case cgltf_attribute_type_texcoord:
            if (primitive->attributes[i].index == 0)
            {
                vertex_streams.push_back(LoadBufferStream(primitive->attributes[i].data, false, vertex_count));
                vertex_types.push_back(primitive->attributes[i].type);
            }
            break;
        case cgltf_attribute_type_normal:
            vertex_streams.push_back(LoadBufferStream(primitive->attributes[i].data, true, vertex_count));
            vertex_types.push_back(primitive->attributes[i].type);
            break;
        case cgltf_attribute_type_tangent:
            vertex_streams.push_back(LoadBufferStream(primitive->attributes[i].data, false, vertex_count));
            vertex_types.push_back(primitive->attributes[i].type);
            break;
        }
    }

    std::vector<unsigned int> remap(index_count);

    void* remapped_indices = RE_ALLOC(indices.stride * index_count);
    std::vector<void*> remapped_vertices;

    size_t remapped_vertex_count;

    switch (indices.stride)
    {
    case 4:
        remapped_vertex_count = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned int*)indices.data, index_count, vertex_count, vertex_streams.data(), vertex_streams.size());
        meshopt_remapIndexBuffer((unsigned int*)remapped_indices, (const unsigned int*)indices.data, index_count, &remap[0]);
        break;
    case 2:
        remapped_vertex_count = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned short*)indices.data, index_count, vertex_count, vertex_streams.data(), vertex_streams.size());
        meshopt_remapIndexBuffer((unsigned short*)remapped_indices, (const unsigned short*)indices.data, index_count, &remap[0]);
        break;
    case 1:
        remapped_vertex_count = meshopt_generateVertexRemapMulti(&remap[0], (const unsigned char*)indices.data, index_count, vertex_count, vertex_streams.data(), vertex_streams.size());
        meshopt_remapIndexBuffer((unsigned char*)remapped_indices, (const unsigned char*)indices.data, index_count, &remap[0]);
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    void* pos_vertices = nullptr;
    size_t pos_stride = 0;

    for (size_t i = 0; i < vertex_streams.size(); ++i)
    {
        void* vertices = RE_ALLOC(vertex_streams[i].stride * remapped_vertex_count);

        meshopt_remapVertexBuffer(vertices, vertex_streams[i].data, vertex_count, vertex_streams[i].stride, &remap[0]);

        remapped_vertices.push_back(vertices);

        if (vertex_types[i] == cgltf_attribute_type_position)
        {
            pos_vertices = vertices;
            pos_stride = vertex_streams[i].stride;
        }
    }

    size_t max_vertices = 64;
    size_t max_triangles = 124;
    const float cone_weight = 0.5f;
    size_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles);

    std::vector<meshopt_Meshlet> meshlets(max_meshlets);
    std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
    std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

    size_t meshlet_count;
    switch (indices.stride)
    {
    case 4:
        meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
            (const unsigned int*)remapped_indices, index_count,
            (const float*)pos_vertices, remapped_vertex_count, pos_stride,
            max_vertices, max_triangles, cone_weight);
        break;
    case 2:
        meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
            (const unsigned short*)remapped_indices, index_count,
            (const float*)pos_vertices, remapped_vertex_count, pos_stride,
            max_vertices, max_triangles, cone_weight);
        break;
    case 1:
        meshlet_count = meshopt_buildMeshlets(meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
            (const unsigned char*)remapped_indices, index_count,
            (const float*)pos_vertices, remapped_vertex_count, pos_stride,
            max_vertices, max_triangles, cone_weight);
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    const meshopt_Meshlet& last = meshlets[meshlet_count - 1];
    meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
    meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
    meshlets.resize(meshlet_count);

    std::vector<unsigned short> meshlet_triangles16;
    meshlet_triangles16.reserve(meshlet_triangles.size());
    for (size_t i = 0; i < meshlet_triangles.size(); ++i)
    {
        meshlet_triangles16.push_back(meshlet_triangles[i]);
    }

    struct MeshletBound
    {
        float3 center;
        float radius;

        union
        {
            //axis + cutoff, rgba8snorm
            struct
            {
                int8_t axis_x;
                int8_t axis_y;
                int8_t axis_z;
                int8_t cutoff;
            };
            uint32_t cone; 
        };

        uint vertexCount;
        uint triangleCount;

        uint vertexOffset;
        uint triangleOffset;
    };
    std::vector<MeshletBound> meshlet_bounds(meshlet_count);

    for (size_t i = 0; i < meshlet_count; ++i)
    {
        const meshopt_Meshlet& m = meshlets[i];

        meshopt_Bounds meshoptBounds = meshopt_computeMeshletBounds(&meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
            m.triangle_count, (const float*)pos_vertices, remapped_vertex_count, pos_stride);

        MeshletBound bound;
        bound.center = float3(meshoptBounds.center);
        bound.radius = meshoptBounds.radius;
        bound.axis_x = meshoptBounds.cone_axis_s8[0];
        bound.axis_y = meshoptBounds.cone_axis_s8[1];
        bound.axis_z = meshoptBounds.cone_axis_s8[2];
        bound.cutoff = meshoptBounds.cone_cutoff_s8;
        bound.vertexCount = m.vertex_count;
        bound.triangleCount = m.triangle_count;
        bound.vertexOffset = m.vertex_offset;
        bound.triangleOffset = m.triangle_offset;
        
        meshlet_bounds[i] = bound;
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    ResourceCache* cache = ResourceCache::GetInstance();

    mesh->m_pRenderer = pRenderer;

    if (indices.stride == 1)
    {
        uint16_t* data = (uint16_t*)RE_ALLOC(sizeof(uint16_t) * index_count);
        for (uint32_t i = 0; i < index_count; ++i)
        {
            data[i] = ((const char*)indices.data)[i];
        }

        indices.stride = 2;

        RE_FREE((void*)indices.data);
        indices.data = data;
    }

    mesh->m_indexBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") IB", remapped_indices, (uint32_t)indices.stride * (uint32_t)index_count);
    mesh->m_indexBufferFormat = indices.stride == 4 ? GfxFormat::R32UI : GfxFormat::R16UI;
    mesh->m_nIndexCount = (uint32_t)index_count;
    mesh->m_nVertexCount = (uint32_t)remapped_vertex_count;

    for (size_t i = 0; i < vertex_types.size(); ++i)
    {
        switch (vertex_types[i])
        {
        case cgltf_attribute_type_position:
            mesh->m_posBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") pos", remapped_vertices[i], (uint32_t)vertex_streams[i].stride * (uint32_t)remapped_vertex_count);
            break;
        case cgltf_attribute_type_texcoord:
            mesh->m_uvBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") UV", remapped_vertices[i], (uint32_t)vertex_streams[i].stride * (uint32_t)remapped_vertex_count);
            break;
        case cgltf_attribute_type_normal:
            mesh->m_normalBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") normal", remapped_vertices[i], (uint32_t)vertex_streams[i].stride * (uint32_t)remapped_vertex_count);
            break;
        case cgltf_attribute_type_tangent:
            mesh->m_tangentBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") tangent", remapped_vertices[i], (uint32_t)vertex_streams[i].stride * (uint32_t)remapped_vertex_count);
            break;
        default:
            break;
        }
    }

    mesh->m_nMeshletCount = (uint32_t)meshlet_count;
    mesh->m_meshletBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") meshlet", meshlet_bounds.data(), sizeof(MeshletBound) * (uint32_t)meshlet_bounds.size());
    mesh->m_meshletVerticesBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") meshlet vertices", meshlet_vertices.data(), sizeof(unsigned int) * (uint32_t)meshlet_vertices.size());
    mesh->m_meshletIndicesBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") meshlet indices", meshlet_triangles16.data(), sizeof(unsigned short) * (uint32_t)meshlet_triangles16.size());

    mesh->Create();
    m_pWorld->AddObject(mesh);

    RE_FREE((void*)indices.data);
    for (size_t i = 0; i < vertex_streams.size(); ++i)
    {
        RE_FREE((void*)vertex_streams[i].data);
    }

    RE_FREE(remapped_indices);
    for (size_t i = 0; i < remapped_vertices.size(); ++i)
    {
        RE_FREE(remapped_vertices[i]);
    }

    return mesh;
}
