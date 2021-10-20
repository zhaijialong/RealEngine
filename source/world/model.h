#pragma once

#include "i_visible_object.h"

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_accessor;

class Model : public IVisibleObject
{
	struct Material
	{
		std::string name;

		Texture2D* albedoTexture = nullptr;
		Texture2D* metallicRoughnessTexture = nullptr;
		Texture2D* normalTexture = nullptr;
		Texture2D* emissiveTexture = nullptr;
		Texture2D* aoTexture = nullptr;

		float3 albedoColor;
		float3 emissiveColor;
		float metallic;
		float roughness;
		float alphaCutoff;
		bool alphaTest;
	};

	struct Mesh
	{
		std::string name;
		std::unique_ptr<Material> material;

		IGfxPipelineState* PSO = nullptr;
		IGfxPipelineState* shadowPSO = nullptr;

		std::unique_ptr<IndexBuffer> indexBuffer;

		std::unique_ptr<StructuredBuffer> posBuffer;
		std::unique_ptr<StructuredBuffer> uvBuffer;
		std::unique_ptr<StructuredBuffer> normalBuffer;
		std::unique_ptr<StructuredBuffer> tangentBuffer;
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
    virtual void Tick(float delta_time) override;
	virtual void Render(Renderer* pRenderer) override;

private:
	void RenderShadowPass(IGfxCommandList* pCommandList, Renderer* pRenderer, const float4x4& mtxVP, Node* pNode, const float4x4& parentWorld);
	void RenderBassPass(IGfxCommandList* pCommandList, Renderer* pRenderer, Camera* pCamera, Node* pNode, const float4x4& parentWorld);

	Texture2D* LoadTexture(const std::string& file, bool srgb);
	Node* LoadNode(const cgltf_node* gltf_node);
	Mesh* LoadMesh(const cgltf_primitive* gltf_primitive, const std::string& name);
	Material* LoadMaterial(const cgltf_material* gltf_material);
	IndexBuffer* LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name);
	StructuredBuffer* LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, bool convertToLH);

	IGfxPipelineState* GetPSO(Material* material);
	IGfxPipelineState* GetShadowPSO(Material* material);

private:
    std::string m_file;

	std::unique_ptr<Node> m_pRootNode;
};
