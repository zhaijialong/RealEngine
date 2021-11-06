#pragma once

#include "i_visible_object.h"

struct cgltf_data;
struct cgltf_node;
struct cgltf_primitive;
struct cgltf_material;
struct cgltf_accessor;
struct cgltf_animation;
struct cgltf_animation_channel;
struct cgltf_skin;

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

		float3 albedoColor = float3(1.0f, 1.0f, 1.0f);
		float3 emissiveColor = float3(0.0f, 0.0f, 0.0f);
		float metallic = 0.0f;
		float roughness = 0.0f;
		float alphaCutoff = 0.0f;
		bool alphaTest = false;
	};

	struct Mesh
	{
		std::string name;
		std::unique_ptr<Material> material;

		IGfxPipelineState* PSO = nullptr;
		IGfxPipelineState* shadowPSO = nullptr;
		IGfxPipelineState* velocityPSO = nullptr;

		std::unique_ptr<IndexBuffer> indexBuffer;

		std::unique_ptr<StructuredBuffer> posBuffer;
		std::unique_ptr<StructuredBuffer> uvBuffer;
		std::unique_ptr<StructuredBuffer> normalBuffer;
		std::unique_ptr<StructuredBuffer> tangentBuffer;
		std::unique_ptr<StructuredBuffer> boneIDBuffer;
		std::unique_ptr<StructuredBuffer> boneWeightBuffer;

		IGfxPipelineState* animPSO = nullptr;

		std::unique_ptr<StructuredBuffer> animPosBuffer;
        std::unique_ptr<StructuredBuffer> animNormalBuffer;
		std::unique_ptr<StructuredBuffer> animTangenetBuffer;

		std::unique_ptr<StructuredBuffer> prevAnimPosBuffer;
	};

	struct AnimationPose
	{
		float3 translation = float3(0.0f, 0.0f, 0.0f);
		float3 scale = float3(1.0f, 1.0f, 1.0f);
		float4 rotation = float4(0.0f, 0.0f, 0.0f, 1.0f);
	};

	struct Node
	{
		std::string name;
		float4x4 localToParentMatrix;
		float4x4 worldMatrix;
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<std::unique_ptr<Node>> childNodes;

		AnimationPose animPose;
		bool isBone = false;
	};

	enum class AnimationChannelMode
	{
		Translation,
		Rotation,
		Scale,
	};

	enum class AnimationInterpolation
	{
		Linear,
		Step,
		CubicSpline,
	};

	struct AnimationChannel
	{
		Node* targetNode;
		AnimationChannelMode mode;
		AnimationInterpolation interpolation;
		std::vector<std::pair<float, float4>> keyframes;
	};

	struct Animation
	{
		std::vector<AnimationChannel> channels;
		float timeDuration = 0.0f;
	};

public:
    virtual void Load(tinyxml2::XMLElement* element) override;
    virtual void Store(tinyxml2::XMLElement* element) override;
    virtual bool Create() override;
    virtual void Tick(float delta_time) override;
	virtual void Render(Renderer* pRenderer) override;

private:
	void AddComputeBuffer(Node* pNode);
	void ComputeAnimation(IGfxCommandList* pCommandList, Node* pNode);
	void RenderShadowPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode);
	void RenderBassPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode);
	void RenderVelocityPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode);

	Texture2D* LoadTexture(const std::string& file, bool srgb);
	Node* LoadNode(const cgltf_node* gltf_node);
	Mesh* LoadMesh(const cgltf_primitive* gltf_primitive, const std::string& name);
	Material* LoadMaterial(const cgltf_material* gltf_material);
	IndexBuffer* LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name);
	StructuredBuffer* LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, bool convertToLH);
	Animation* LoadAnimation(const cgltf_animation* gltf_animation);
	void LoadAnimationChannel(const cgltf_animation_channel* gltf_channel, AnimationChannel& channel);
	void LoadSkin(const cgltf_skin* skin);

	void UpdateMatrix(Node* node, const float4x4& parentWorld);
	void PlayAnimationChannel(const AnimationChannel& channel);
	void LinearInterpolate(const AnimationChannel& channel, float interpolation_value, const float4& lower, const float4& upper);

	IGfxPipelineState* GetPSO(Material* material);
	IGfxPipelineState* GetShadowPSO(Material* material);
	IGfxPipelineState* GetVelocityPSO(Material* material);

private:
    std::string m_file;

	std::unique_ptr<Node> m_pRootNode;
	std::unordered_map<const cgltf_node*, Node*> m_nodeMapping;

	std::unique_ptr<Animation> m_pAnimation;
	std::vector<float4x4> m_inverseBindMatrices;
	std::vector<Node*> m_boneList;
	std::vector<float4x4> m_boneMatrices;
	std::unique_ptr<RawBuffer> m_pBoneMatrixBuffer;
	uint32_t m_boneMatrixBufferOffset = 0;
	uint32_t m_prevBoneMatrixBufferOffset = 0;
	float m_currentAnimTime = 0.0f;
};
