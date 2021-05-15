#pragma once

#include "i_visible_object.h"
#include "renderer/renderer.h"
#include "utils/math.h"

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_accessor;

class Model : public IVisibleObject
{
	struct Material
	{
		//todo
	};

	struct Mesh
	{
		std::string name;
		std::unique_ptr<Material> material;
		std::unique_ptr<IGfxBuffer> indexBuffer;
		std::unique_ptr<IGfxBuffer> posBuffer;
		std::unique_ptr<IGfxDescriptor> posBufferSRV;
		//todo normal, uv buffer
	};

	struct Node
	{
		std::string name;
		float4x4 localToParentMatrix;
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<std::unique_ptr<Node>> childNodes;
	};

public:
    virtual void Load(tinyxml2::XMLElement* element) override;
    virtual void Store(tinyxml2::XMLElement* element) override;
    virtual bool Create() override;
    virtual void Tick() override;

private:
    void LoadTextures(const cgltf_data* gltf_data);
	Node* LoadNode(const cgltf_node* gltf_node);
	Mesh* LoadMesh(const cgltf_primitive* gltf_primitive, const std::string& name);
	Material* LoadMaterial(const cgltf_material* gltf_material);
	IGfxBuffer* LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name);
	void LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, IGfxBuffer** buffer, IGfxDescriptor** descriptor);

private:
    std::string m_file;

	std::unique_ptr<Node> m_pRootNode;
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
};
