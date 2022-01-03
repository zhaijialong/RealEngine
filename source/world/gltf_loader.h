#pragma once

#include "utils/math.h"

class World;
class StaticMesh;
class MeshMaterial;
class Texture2D;

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_texture_view;

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
    void LoadNode(cgltf_data* data, cgltf_node* node, const float4x4& mtxParentToWorld);
    StaticMesh* LoadMesh(cgltf_primitive* primitive, const std::string& name);
    MeshMaterial* LoadMaterial(cgltf_material* gltf_material);
    Texture2D* LoadTexture(const cgltf_texture_view& texture_view, bool srgb);

private:
    World* m_pWorld = nullptr;
    std::string m_file;

    float4x4 m_mtxWorld;
};