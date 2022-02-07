#include "gltf_loader.h"
#include "static_mesh.h"
#include "skeletal_mesh.h"
#include "animation.h"
#include "skeleton.h"
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

inline void GetTransform(const cgltf_node* node, float4x4& matrix)
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

inline bool IsFrontFaceCCW(const cgltf_node* node)
{
    //gltf 2.0 spec : If the determinant is a positive value, the winding order triangle faces is counterclockwise; in the opposite case, the winding order is clockwise.

    cgltf_float m[16];
    cgltf_node_transform_world(node, m);

    float4x4 matrix(m);

    return determinant(matrix) > 0.0f;
}

inline uint32_t GetMeshIndex(const cgltf_data* data, const cgltf_mesh* mesh)
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

inline uint32_t GetNodeIndex(const cgltf_data* data, const cgltf_node* node)
{
    for (cgltf_size i = 0; i < data->nodes_count; ++i)
    {
        if (&data->nodes[i] == node)
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

    const tinyxml2::XMLAttribute* position_attr = element->FindAttribute("position");
    if (position_attr)
    {
        m_position = str_to_float3(position_attr->Value());
    }

    const tinyxml2::XMLAttribute* rotation_attr = element->FindAttribute("rotation");
    if (rotation_attr)
    {
        m_rotation = str_to_float3(rotation_attr->Value());
    }

    const tinyxml2::XMLAttribute* scale_attr = element->FindAttribute("scale");
    if (scale_attr)
    {
        m_scale = str_to_float3(scale_attr->Value());
    }

    float4x4 T = translation_matrix(m_position);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 S = scaling_matrix(m_scale);
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

    if (data->animations_count > 0)
    {
        SkeletalMesh* mesh = new SkeletalMesh(m_file);
        mesh->m_pRenderer = Engine::GetInstance()->GetRenderer();
        mesh->m_pAnimation.reset(LoadAnimation(data, &data->animations[0])); //currently only load the first one
        mesh->m_pSkeleton.reset(LoadSkeleton(data, &data->skins[0]));

        for (cgltf_size i = 0; i < data->nodes_count; ++i)
        {
            mesh->m_nodes.emplace_back(LoadSkeletalMeshNode(data, &data->nodes[i]));
        }

        for (cgltf_size i = 0; i < data->scene->nodes_count; ++i)
        {
            mesh->m_rootNodes.push_back(GetNodeIndex(data, data->scene->nodes[i]));
        }

        mesh->SetPosition(m_position);
        mesh->SetRotation(m_rotation);
        mesh->SetScale(m_scale);
        mesh->Create();
        m_pWorld->AddObject(mesh);
    }
    else
    {
        for (cgltf_size i = 0; i < data->scenes_count; ++i)
        {
            for (cgltf_size node = 0; node < data->scenes[i].nodes_count; ++node)
            {
                LoadStaticMeshNode(data, data->scenes[i].nodes[node], m_mtxWorld);
            }
        }
    }

    cgltf_free(data);
}

void GLTFLoader::LoadStaticMeshNode(const cgltf_data* data, const cgltf_node* node, const float4x4& mtxParentToWorld)
{
    float4x4 mtxLocalToParent;
    GetTransform(node, mtxLocalToParent);

    float4x4 mtxLocalToWorld = mul(mtxParentToWorld, mtxLocalToParent);

    if (node->mesh)
    {
        float3 position;
        float3 rotation;
        float3 scale;
        decompose(mtxLocalToWorld, position, rotation, scale);

        uint32_t mesh_index = GetMeshIndex(data, node->mesh);
        bool bFrontFaceCCW = IsFrontFaceCCW(node);

        for (cgltf_size i = 0; i < node->mesh->primitives_count; i++)
        {
            std::string name = "mesh_" + std::to_string(mesh_index) + "_" + std::to_string(i) + " " + (node->mesh->name ? node->mesh->name : "");

            StaticMesh* mesh = LoadStaticMesh(&node->mesh->primitives[i], name);

            mesh->m_pMaterial->m_bFrontFaceCCW = bFrontFaceCCW;
            mesh->SetPosition(position);
            mesh->SetRotation(rotation);
            mesh->SetScale(scale);
        }
    }

    for (cgltf_size i = 0; i < node->children_count; ++i)
    {
        LoadStaticMeshNode(data, node->children[i], mtxLocalToWorld);
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

MeshMaterial* GLTFLoader::LoadMaterial(const cgltf_material* gltf_material)
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
    material->m_bAlphaBlend = gltf_material->alpha_mode == cgltf_alpha_mode_blend;
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

StaticMesh* GLTFLoader::LoadStaticMesh(const cgltf_primitive* primitive, const std::string& name)
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

Animation* GLTFLoader::LoadAnimation(const cgltf_data* data, const cgltf_animation* gltf_animation)
{
    Animation* animation = new Animation(gltf_animation->name ? gltf_animation->name : "");

    animation->m_channels.reserve(gltf_animation->channels_count);
    for (cgltf_size i = 0; i < gltf_animation->channels_count; ++i)
    {
        const cgltf_animation_channel* gltf_channel = &gltf_animation->channels[i];

        AnimationChannel channel;
        channel.targetNode = GetNodeIndex(data, gltf_channel->target_node);

        switch (gltf_channel->target_path)
        {
        case cgltf_animation_path_type_translation:
            channel.mode = AnimationChannelMode::Translation;
            break;
        case cgltf_animation_path_type_rotation:
            channel.mode = AnimationChannelMode::Rotation;
            break;
        case cgltf_animation_path_type_scale:
            channel.mode = AnimationChannelMode::Scale;
            break;
        default:
            RE_ASSERT(false);
            break;
        }

        const cgltf_animation_sampler* sampler = gltf_channel->sampler;
        RE_ASSERT(sampler->interpolation == cgltf_interpolation_type_linear); //currently only support linear sampler

        cgltf_accessor* time_accessor = sampler->input;
        cgltf_accessor* value_accessor = sampler->output;
        RE_ASSERT(time_accessor->count == value_accessor->count);

        cgltf_size keyframe_num = time_accessor->count;
        channel.keyframes.reserve(keyframe_num);

        char* time_data = (char*)time_accessor->buffer_view->buffer->data + time_accessor->buffer_view->offset + time_accessor->offset;
        char* value_data = (char*)value_accessor->buffer_view->buffer->data + value_accessor->buffer_view->offset + value_accessor->offset;

        for (cgltf_size k = 0; k < keyframe_num; ++k)
        {
            float time;
            float4 value;

            memcpy(&time, time_data + time_accessor->stride * k, time_accessor->stride);
            memcpy(&value, value_data + value_accessor->stride * k, value_accessor->stride);

            channel.keyframes.push_back(std::make_pair(time, value));
        }

        animation->m_channels.push_back(channel);
    }

    for (size_t i = 0; i < animation->m_channels.size(); ++i)
    {
        for (size_t j = 0; j < animation->m_channels[i].keyframes.size(); ++j)
        {
            animation->m_timeDuration = max(animation->m_channels[i].keyframes[j].first, animation->m_timeDuration);
        }
    }

    return animation;
}

Skeleton* GLTFLoader::LoadSkeleton(const cgltf_data* data, const cgltf_skin* skin)
{
    if (skin == nullptr)
    {
        return nullptr;
    }

    Skeleton* skeleton = new Skeleton(skin->name ? skin->name : "");
    skeleton->m_joints.resize(skin->joints_count);
    skeleton->m_inverseBindMatrices.resize(skin->joints_count);
    skeleton->m_jointMatrices.resize(skin->joints_count);

    for (cgltf_size i = 0; i < skin->joints_count; ++i)
    {
        skeleton->m_joints[i] = GetNodeIndex(data, skin->joints[i]);
    }

    const cgltf_accessor* accessor = skin->inverse_bind_matrices;
    RE_ASSERT(accessor->count == skin->joints_count);

    uint32_t size = (uint32_t)accessor->stride * (uint32_t)accessor->count;

    std::vector<float4x4> inverseBindMatrices(accessor->count);
    memcpy(inverseBindMatrices.data(), (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset, size);

    for (cgltf_size i = 0; i < accessor->count; ++i)
    {
        float3 translation;
        float4 rotation;
        float3 scale;
        decompose(inverseBindMatrices[i], translation, rotation, scale);

        //right-hand to left-hand
        translation.z *= -1;
        rotation.z *= -1;
        rotation.w *= -1;

        float4x4 T = translation_matrix(translation);
        float4x4 R = rotation_matrix(rotation);
        float4x4 S = scaling_matrix(scale);

        skeleton->m_inverseBindMatrices[i] = mul(T, mul(R, S));
    }

    return skeleton;
}

SkeletalMeshNode* GLTFLoader::LoadSkeletalMeshNode(const cgltf_data* data, const cgltf_node* gltf_node)
{
    SkeletalMeshNode* node = new SkeletalMeshNode;
    node->id = GetNodeIndex(data, gltf_node);
    node->name = gltf_node->name ? gltf_node->name : "node_" + std::to_string(node->id);
    node->parent = gltf_node->parent ? GetNodeIndex(data, gltf_node->parent) : -1;
    for (cgltf_size i = 0; i < gltf_node->children_count; ++i)
    {
        node->children.push_back(GetNodeIndex(data, gltf_node->children[i]));
    }

    if (gltf_node->has_matrix)
    {
        float4x4 matrix = float4x4(gltf_node->matrix);
        decompose(matrix, node->translation, node->rotation, node->scale);
    }
    else
    {
        node->translation = float3(gltf_node->translation);
        node->rotation = float4(gltf_node->rotation);
        node->scale = float3(gltf_node->scale);
    }

    //right-hand to left-hand
    node->translation.z *= -1;
    node->rotation.z *= -1;
    node->rotation.w *= -1;
    
    if (gltf_node->mesh)
    {
        bool vertex_skinning = gltf_node->skin != nullptr;
        uint32_t mesh_index = GetMeshIndex(data, gltf_node->mesh);
        bool bFrontFaceCCW = IsFrontFaceCCW(gltf_node);

        for (cgltf_size i = 0; i < gltf_node->mesh->primitives_count; i++)
        {
            std::string name = "mesh_" + std::to_string(mesh_index) + "_" + std::to_string(i) + " " + (gltf_node->mesh->name ? gltf_node->mesh->name : "");

            SkeletalMeshData* mesh = LoadSkeletalMesh(&gltf_node->mesh->primitives[i], name);
            mesh->nodeID = node->id;
            mesh->material->m_bSkeletalAnim = vertex_skinning;
            mesh->material->m_bFrontFaceCCW = bFrontFaceCCW;

            node->meshes.emplace_back(mesh);
        }
    }

    return node;
}

SkeletalMeshData* GLTFLoader::LoadSkeletalMesh(const cgltf_primitive* primitive, const std::string& name)
{
    SkeletalMeshData* mesh = new SkeletalMeshData;
    mesh->name = m_file + " " + name;
    mesh->material.reset(LoadMaterial(primitive->material));

    ResourceCache* cache = ResourceCache::GetInstance();

    size_t index_count;
    meshopt_Stream indices = LoadBufferStream(primitive->indices, false, index_count);

    mesh->indexBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") IB", indices.data, (uint32_t)indices.stride * (uint32_t)index_count);
    mesh->indexBufferFormat = indices.stride == 4 ? GfxFormat::R32UI : GfxFormat::R16UI;
    mesh->indexCount = (uint32_t)index_count;

    size_t vertex_count;
    meshopt_Stream vertices;

    for (cgltf_size i = 0; i < primitive->attributes_count; ++i)
    {
        switch (primitive->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            vertices = LoadBufferStream(primitive->attributes[i].data, true, vertex_count);
            mesh->staticPosBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") pos", vertices.data, (uint32_t)vertices.stride * (uint32_t)vertex_count);
            break;
        case cgltf_attribute_type_texcoord:
            if (primitive->attributes[i].index == 0)
            {
                vertices = LoadBufferStream(primitive->attributes[i].data, false, vertex_count);
                mesh->uvBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") UV", vertices.data, (uint32_t)vertices.stride * (uint32_t)vertex_count);
            }
            break;
        case cgltf_attribute_type_normal:
            vertices = LoadBufferStream(primitive->attributes[i].data, true, vertex_count);
            mesh->staticNormalBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") normal", vertices.data, (uint32_t)vertices.stride * (uint32_t)vertex_count);
            break;
        case cgltf_attribute_type_tangent:
            vertices = LoadBufferStream(primitive->attributes[i].data, false, vertex_count);
            mesh->staticTangentBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") tangent", vertices.data, (uint32_t)vertices.stride * (uint32_t)vertex_count);
            break;
        case cgltf_attribute_type_joints:
        {
            const cgltf_accessor* accessor = primitive->attributes[i].data;

            std::vector<ushort4> jointIDs;
            jointIDs.reserve(accessor->count * 4);

            for (cgltf_size j = 0; j < accessor->count; ++j)
            {
                cgltf_uint id[4];
                cgltf_accessor_read_uint(accessor, j, id, 4);

                jointIDs.push_back(ushort4(id[0], id[1], id[2], id[3]));
            }

            mesh->jointIDBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") joint ID", jointIDs.data(), sizeof(ushort4) * (uint32_t)accessor->count);
            break;
        }
        case cgltf_attribute_type_weights:
        {
            const cgltf_accessor* accessor = primitive->attributes[i].data;

            std::vector<float4> jointWeights;
            jointWeights.reserve(accessor->count);

            for (cgltf_size j = 0; j < accessor->count; ++j)
            {
                cgltf_float weight[4];
                cgltf_accessor_read_float(accessor, j, weight, 4);

                jointWeights.push_back(float4(weight));
            }

            mesh->jointWeightBufferAddress = cache->GetSceneBuffer("model(" + m_file + " " + name + ") joint weight", jointWeights.data(), sizeof(float4) * (uint32_t)accessor->count);
            break;
        }
        }
    }

    mesh->vertexCount = (uint32_t)vertex_count;

    return mesh;
}
