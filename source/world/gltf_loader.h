#pragma once

#include "utils/math.h"

class World;
class StaticMesh;
class MeshMaterial;
class Texture2D;
class Animation;
struct SkeletalMeshNode;
struct SkeletalMeshData;

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture_view;
struct cgltf_animation;

namespace tinyxml2 
{
    class XMLElement;
}

class GLTFLoader
{
public:
    GLTFLoader(World* world, tinyxml2::XMLElement* element);

    void Load();
    
private:
    void LoadStaticMeshNode(cgltf_data* data, cgltf_node* node, const float4x4& mtxParentToWorld);
    StaticMesh* LoadStaticMesh(cgltf_primitive* primitive, const std::string& name);

    Animation* LoadAnimation(const cgltf_data* data, const cgltf_animation* animation);
    SkeletalMeshNode* LoadSkeletalMeshNode(const cgltf_data* data, const cgltf_node* node);
    SkeletalMeshData* LoadSkeletalMesh(const cgltf_primitive* primitive, const std::string& name);

    MeshMaterial* LoadMaterial(cgltf_material* gltf_material);
    Texture2D* LoadTexture(const cgltf_texture_view& texture_view, bool srgb);

private:
    World* m_pWorld = nullptr;
    std::string m_file;

    float3 m_position = float3(0, 0, 0);
    float3 m_rotation = float3(0, 0, 0);
    float3 m_scale = float3(1, 1, 1);
    float4x4 m_mtxWorld;
};