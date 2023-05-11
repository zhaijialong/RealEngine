#pragma once

#include "utils/math.h"
#include "EASTL/string.h"

class World;
class StaticMesh;
class MeshMaterial;
class Texture2D;
class Animation;
class Skeleton;
struct SkeletalMeshNode;
struct SkeletalMeshData;

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture_view;
struct cgltf_animation;
struct cgltf_skin;

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
    void LoadStaticMeshNode(const cgltf_data* data, const cgltf_node* node, const float4x4& mtxParentToWorld);
    StaticMesh* LoadStaticMesh(const cgltf_primitive* primitive, const eastl::string& name, bool bFrontFaceCCW);

    Animation* LoadAnimation(const cgltf_data* data, const cgltf_animation* animation);
    Skeleton* LoadSkeleton(const cgltf_data* data, const cgltf_skin* skin);
    SkeletalMeshNode* LoadSkeletalMeshNode(const cgltf_data* data, const cgltf_node* node);
    SkeletalMeshData* LoadSkeletalMesh(const cgltf_primitive* primitive, const eastl::string& name);

    MeshMaterial* LoadMaterial(const cgltf_material* gltf_material);
    Texture2D* LoadTexture(const cgltf_texture_view& texture_view, bool srgb);

private:
    World* m_pWorld = nullptr;
    eastl::string m_file;

    float3 m_position = float3(0, 0, 0);
    quaternion m_rotation = quaternion(0, 0, 0, 1);
    float3 m_scale = float3(1, 1, 1);
    float4x4 m_mtxWorld;

    eastl::string m_anisotropicTexture;
};