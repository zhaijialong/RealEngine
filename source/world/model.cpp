#include "model.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "utils/memory.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#include "model_constants.hlsli"

inline float4x4 RightHandToLeftHand(const float4x4& matrix)
{
    return float4x4(float4(matrix.x.x, matrix.x.y, -matrix.x.z, matrix.x.w),
        float4(matrix.y.x, matrix.y.y, -matrix.y.z, matrix.y.w),
        float4(-matrix.z.x, -matrix.z.y, matrix.z.z, -matrix.z.w),
        matrix.w);
}

void Model::Load(tinyxml2::XMLElement* element)
{
    IVisibleObject::Load(element);

    m_file = element->FindAttribute("file")->Value();
}

void Model::Store(tinyxml2::XMLElement* element)
{
    IVisibleObject::Store(element);
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

    RE_ASSERT(data->scenes_count == 1 && data->scenes[0].nodes_count == 1);
    m_pRootNode.reset(LoadNode(data->scenes[0].nodes[0]));

    if (data->animations_count > 0)
    {
        m_pAnimation.reset(LoadAnimation(&data->animations[0]));
        LoadSkin(&data->skins[0]);
    }

    cgltf_free(data);
    m_nodeMapping.clear();
    return true;
}

void Model::Tick(float delta_time)
{
    if (m_pAnimation != nullptr)
    {
        m_currentAnimTime += delta_time;
        if (m_currentAnimTime > m_pAnimation->timeDuration)
        {
            m_currentAnimTime = m_currentAnimTime - m_pAnimation->timeDuration;
        }

        for (size_t i = 0; i < m_pAnimation->channels.size(); ++i)
        {
            const AnimationChannel& channel = m_pAnimation->channels[i];
            PlayAnimationChannel(channel);
        }
    }

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 S = scaling_matrix(m_scale);
    float4x4 mtxWorld = mul(T, mul(R, S));
    UpdateMatrix(m_pRootNode.get(), mtxWorld);

    if (!m_boneList.empty())
    {
        m_boneMatrices.resize(m_boneList.size());

        const float4x4 inverse_model = inverse(m_pRootNode->localToParentMatrix);

        for (size_t i = 0; i < m_boneList.size(); ++i)
        {
            m_boneMatrices[i] = mul(inverse_model, mul(m_boneList[i]->worldMatrix, m_inverseBindMatrices[i]));
        }


        void* data = m_pBoneMatrixBuffer->GetBuffer()->GetCpuAddress();
        uint32_t size = sizeof(float4x4) * (uint32_t)m_boneMatrices.size();

        m_boneMatrixBufferOffset += size;
        if (m_boneMatrixBufferOffset >= m_pBoneMatrixBuffer->GetBuffer()->GetDesc().size)
        {
            m_boneMatrixBufferOffset = 0;
        }

        memcpy((char*)data + m_boneMatrixBufferOffset, m_boneMatrices.data(), size);        
    }
}

void Model::UpdateMatrix(Node* node, const float4x4& parentWorld)
{
    node->prevWorldMatrix = node->worldMatrix;

    if (node->isBone)
    {
        float4x4 T = translation_matrix(node->animPose.translation);
        float4x4 R = rotation_matrix(node->animPose.rotation);
        float4x4 S = scaling_matrix(node->animPose.scale);
        float4x4 poseMatrix = RightHandToLeftHand(mul(T, mul(R, S)));
        node->worldMatrix = mul(parentWorld, poseMatrix);
    }
    else
    {
        node->worldMatrix = mul(parentWorld, node->localToParentMatrix);
    }

    for (size_t i = 0; i < node->childNodes.size(); ++i)
    {
        UpdateMatrix(node->childNodes[i].get(), node->worldMatrix);
    }
}

void Model::PlayAnimationChannel(const AnimationChannel& channel)
{
    std::pair<float, float4> lower_frame;
    std::pair<float, float4> upper_frame;

    bool found = false;
    for (size_t frame = 0; frame < channel.keyframes.size() - 1; ++frame)
    {
        if (channel.keyframes[frame].first <= m_currentAnimTime &&
            channel.keyframes[frame + 1].first >= m_currentAnimTime)
        {
            lower_frame = channel.keyframes[frame];
            upper_frame = channel.keyframes[frame + 1];
            found = true;
            break;
        }
    }

    float interpolation_value;
    if (found)
    {
        interpolation_value = (m_currentAnimTime - lower_frame.first) / (upper_frame.first - lower_frame.first);
    }
    else
    {
        lower_frame = upper_frame = channel.keyframes[0];
        interpolation_value = 0.0f;
    }

    switch (channel.interpolation)
    {
    case AnimationInterpolation::Linear:
    case AnimationInterpolation::Step:
    case AnimationInterpolation::CubicSpline:
        //todo step, cubicspline
        LinearInterpolate(channel, interpolation_value, lower_frame.second, upper_frame.second);
        break;
    default:
        break;
    }
}

void Model::LinearInterpolate(const AnimationChannel& channel, float interpolation_value, const float4& lower, const float4& upper)
{
    switch (channel.mode)
    {
    case AnimationChannelMode::Translation:
        channel.targetNode->animPose.translation = lerp(lower.xyz(), upper.xyz(), interpolation_value);
        break;
    case AnimationChannelMode::Rotation:
        channel.targetNode->animPose.rotation = slerp(lower, upper, interpolation_value);
        break;
    case AnimationChannelMode::Scale:
        channel.targetNode->animPose.scale = lerp(lower.xyz(), upper.xyz(), interpolation_value);
        break;
    default:
        break;
    }
}

void Model::Render(Renderer* pRenderer)
{
    if (m_pAnimation != nullptr)
    {
        ComputeFunc computePass = std::bind(&Model::ComputeAnimation, this, std::placeholders::_1, m_pRootNode.get());
        pRenderer->AddComputePass(computePass);

        AddComputeBuffer(m_pRootNode.get());
    }

    RenderFunc bassPassBatch = std::bind(&Model::RenderBassPass, this, std::placeholders::_1, std::placeholders::_2, m_pRootNode.get());
    pRenderer->AddGBufferPassBatch(bassPassBatch);

    RenderFunc shadowPassBatch = std::bind(&Model::RenderShadowPass, this, std::placeholders::_1, std::placeholders::_2, m_pRootNode.get());
    pRenderer->AddShadowPassBatch(shadowPassBatch);

    if (m_pAnimation != nullptr) //todo : moved models (by gizmo or something else)
    {
        RenderFunc velocityPassBatch = std::bind(&Model::RenderVelocityPass, this, std::placeholders::_1, std::placeholders::_2, m_pRootNode.get());
        pRenderer->AddVelocityPassBatch(velocityPassBatch);
    }
}

void Model::AddComputeBuffer(Node* pNode)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        const GfxBufferDesc& bufferDesc = mesh->posBuffer->GetBuffer()->GetDesc();
        uint32_t vertex_count = bufferDesc.size / bufferDesc.stride;

        if (mesh->animPosBuffer == nullptr)
        {
            mesh->animPosBuffer.reset(pRenderer->CreateStructuredBuffer(nullptr, bufferDesc.stride, vertex_count, mesh->name + " anim pos", GfxMemoryType::GpuOnly, true));
            mesh->prevAnimPosBuffer.reset(pRenderer->CreateStructuredBuffer(nullptr, bufferDesc.stride, vertex_count, mesh->name + " anim pos", GfxMemoryType::GpuOnly, true));
        }

        std::swap(mesh->animPosBuffer, mesh->prevAnimPosBuffer);

        pRenderer->AddComputeBuffer(mesh->animPosBuffer->GetBuffer());
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        AddComputeBuffer(pNode->childNodes[i].get());
    }
}

void Model::ComputeAnimation(IGfxCommandList* pCommandList, Node* pNode)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();

        std::string event_name = m_file + " " + mesh->name;
        GPU_EVENT_DEBUG(pCommandList, event_name.c_str());

        if (mesh->animPSO == nullptr)
        {
            GfxComputePipelineDesc desc;
            desc.cs = pRenderer->GetShader("compute_animation.hlsl", "skeletal_anim_main", "cs_6_6", {});
            mesh->animPSO = pRenderer->GetPipelineState(desc, "Model anim PSO");
        }

        const GfxBufferDesc& bufferDesc = mesh->posBuffer->GetBuffer()->GetDesc();
        uint32_t vertex_count = bufferDesc.size / bufferDesc.stride;

        pCommandList->SetPipelineState(mesh->animPSO);

        uint32_t cb[8] = { 
            mesh->posBuffer->GetSRV()->GetHeapIndex(),
            mesh->animPosBuffer->GetUAV()->GetHeapIndex(), 
            0, 
            0,
            mesh->boneIDBuffer->GetSRV()->GetHeapIndex(),
            mesh->boneWeightBuffer->GetSRV()->GetHeapIndex(),
            m_pBoneMatrixBuffer->GetSRV()->GetHeapIndex(),
            m_boneMatrixBufferOffset,
        };
        pCommandList->SetComputeConstants(1, cb, sizeof(cb));

        pCommandList->Dispatch((vertex_count + 63) / 64, 1, 1);
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        ComputeAnimation(pCommandList, pNode->childNodes[i].get());
    }
}

void Model::RenderShadowPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode)
{
    float4x4 mtxWorld = pNode->worldMatrix;
    float4x4 mtxWVP = mul(mtxVP, mtxWorld);

    ModelConstant modelCB;
    modelCB.mtxWVP = mtxWVP;
    modelCB.mtxWorld = mtxWorld;
    modelCB.mtxNormal = transpose(inverse(mtxWorld));

    pCommandList->SetGraphicsConstants(1, &modelCB, sizeof(modelCB));

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        Material* material = mesh->material.get();

        std::string event_name = m_file + " " + mesh->name;
        GPU_EVENT_DEBUG(pCommandList, event_name.c_str());

        if (mesh->shadowPSO == nullptr)
        {
            mesh->shadowPSO = GetShadowPSO(mesh->material.get());
        }

        pCommandList->SetPipelineState(mesh->shadowPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer->GetBuffer());

        struct CB0
        {
            uint c_posBuffer;
            uint c_uvBuffer;
            uint c_albedoTexture;
            float c_alphaCutoff;
        };

        CB0 cb =
        {
            mesh->animPSO ? mesh->animPosBuffer->GetSRV()->GetHeapIndex() : mesh->posBuffer->GetSRV()->GetHeapIndex(),
            mesh->uvBuffer ? mesh->uvBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
            material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
            material->alphaCutoff
        };
        pCommandList->SetGraphicsConstants(0, &cb, sizeof(cb));

        pCommandList->DrawIndexed(mesh->indexBuffer->GetIndexCount());
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        RenderShadowPass(pCommandList, mtxVP, pNode->childNodes[i].get());
    }
}

void Model::RenderBassPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode)
{
    float4x4 mtxWorld = pNode->worldMatrix;
    float4x4 mtxWVP = mul(mtxVP, mtxWorld);

    ModelConstant modelCB;
    modelCB.mtxWVP = mtxWVP;
    modelCB.mtxWorld = mtxWorld;
    modelCB.mtxNormal = transpose(inverse(mtxWorld));

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        Material* material = mesh->material.get();

        std::string event_name = m_file + " " + mesh->name;
        GPU_EVENT_DEBUG(pCommandList, event_name.c_str());

        if (mesh->PSO == nullptr)
        {
            mesh->PSO = GetPSO(mesh->material.get());
        }

        pCommandList->SetPipelineState(mesh->PSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer->GetBuffer());

        if (mesh->animPSO != nullptr)
        {
            modelCB.posBuffer = mesh->animPosBuffer->GetSRV()->GetHeapIndex();
        }
        else
        {
            modelCB.posBuffer = mesh->posBuffer->GetSRV()->GetHeapIndex();
        }
        modelCB.uvBuffer = mesh->uvBuffer ? mesh->uvBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        modelCB.normalBuffer = mesh->normalBuffer ? mesh->normalBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        modelCB.tangentBuffer = mesh->tangentBuffer ? mesh->tangentBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        pCommandList->SetGraphicsConstants(1, &modelCB, sizeof(modelCB));

        MaterialConstant materialCB;
        materialCB.albedoTexture = material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.metallicRoughnessTexture = material->metallicRoughnessTexture ? material->metallicRoughnessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.normalTexture = material->normalTexture ? material->normalTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.emissiveTexture = material->emissiveTexture ? material->emissiveTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.aoTexture = material->aoTexture ? material->aoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.albedo = material->albedoColor;
        materialCB.emissive = material->emissiveColor;
        materialCB.metallic = material->metallic;
        materialCB.roughness = material->roughness;
        materialCB.alphaCutoff = material->alphaCutoff;
        pCommandList->SetGraphicsConstants(2, &materialCB, sizeof(materialCB));

        pCommandList->DrawIndexed(mesh->indexBuffer->GetIndexCount());
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        RenderBassPass(pCommandList, mtxVP, pNode->childNodes[i].get());
    }
}

void Model::RenderVelocityPass(IGfxCommandList* pCommandList, const float4x4& mtxVP, Node* pNode)
{
    float4x4 mtxWorld = pNode->worldMatrix;
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        Material* material = mesh->material.get();

        std::string event_name = m_file + " " + mesh->name;
        GPU_EVENT_DEBUG(pCommandList, event_name.c_str());

        if (mesh->velocityPSO == nullptr)
        {
            mesh->velocityPSO = GetVelocityPSO(mesh->material.get());
        }

        pCommandList->SetPipelineState(mesh->velocityPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer->GetBuffer());

        struct CB
        {
            uint posBuffer;
            uint prevPosBuffer;
            uint uvBuffer;
            uint albedoTexture;

            float alphaCutoff;
            float3 _padding;

            float4x4 mtxWVP;
            float4x4 mtxWVPNoJitter;
            float4x4 mtxPrevWVPNoJitter;
        };

        CB cb;
        cb.posBuffer = mesh->animPosBuffer ? mesh->animPosBuffer->GetSRV()->GetHeapIndex() : mesh->posBuffer->GetSRV()->GetHeapIndex();
        cb.uvBuffer = mesh->uvBuffer ? mesh->uvBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        cb.prevPosBuffer = mesh->prevAnimPosBuffer ? mesh->prevAnimPosBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        cb.albedoTexture = material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        cb.alphaCutoff = material->alphaCutoff;
        cb.mtxWVP = mul(mtxVP, mtxWorld);
        cb.mtxWVPNoJitter = mul(camera->GetNonJitterViewProjectionMatrix(), mtxWorld);
        cb.mtxPrevWVPNoJitter = mul(camera->GetNonJitterPrevViewProjectionMatrix(), pNode->prevWorldMatrix);

        pCommandList->SetGraphicsConstants(1, &cb, sizeof(cb));


        pCommandList->DrawIndexed(mesh->indexBuffer->GetIndexCount());
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        RenderVelocityPass(pCommandList, mtxVP, pNode->childNodes[i].get());
    }
}

Texture2D* Model::LoadTexture(const std::string& file, bool srgb)
{
    size_t last_slash = m_file.find_last_of('/');
    std::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, last_slash + 1);

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    Texture2D* texture = pRenderer->CreateTexture2D(path + file, srgb);

    return texture;
}

Model::Node* Model::LoadNode(const cgltf_node* gltf_node)
{
    if (gltf_node == nullptr) return nullptr;

    Node* node = new Node;
    node->name = gltf_node->name != nullptr ? gltf_node->name : "";

    if (gltf_node->has_matrix)
    {
        node->localToParentMatrix = float4x4(gltf_node->matrix);
    }
    else
    {
        float4x4 T = translation_matrix(float3(gltf_node->translation));
        float4x4 R = rotation_matrix(float4(gltf_node->rotation));
        float4x4 S = scaling_matrix(float3(gltf_node->scale));
        node->localToParentMatrix = mul(T, mul(R, S));
    }

    node->localToParentMatrix = RightHandToLeftHand(node->localToParentMatrix);

    if (gltf_node->mesh != nullptr)
    {
        for (cgltf_size i = 0; i < gltf_node->mesh->primitives_count; ++i)
        {
            Mesh* mesh = LoadMesh(&gltf_node->mesh->primitives[i], gltf_node->mesh->name ? gltf_node->mesh->name : "");
            node->meshes.push_back(std::unique_ptr<Mesh>(mesh));
        }
    }

    for (cgltf_size i = 0; i < gltf_node->children_count; ++i)
    {
        Node* child = LoadNode(gltf_node->children[i]);
        node->childNodes.push_back(std::unique_ptr<Node>(child));
    }

    RE_ASSERT(m_nodeMapping.find(gltf_node) == m_nodeMapping.end());
    m_nodeMapping.insert(std::make_pair(gltf_node, node));

    return node;
}

Model::Mesh* Model::LoadMesh(const cgltf_primitive* gltf_primitive, const std::string& name)
{
    if (gltf_primitive == nullptr) return nullptr;

    Mesh* mesh = new Mesh;
    mesh->name = name;
    mesh->material.reset(LoadMaterial(gltf_primitive->material));
    mesh->indexBuffer.reset(LoadIndexBuffer(gltf_primitive->indices, "model(" + m_file + " " + name + ") IB"));
    
    for (cgltf_size i = 0; i < gltf_primitive->attributes_count; ++i)
    {
        switch (gltf_primitive->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            mesh->posBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") pos", true));
            break;
        case cgltf_attribute_type_texcoord:
            if (gltf_primitive->attributes[i].index == 0)
            {
                mesh->uvBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") UV", false));
            }
            break;
        case cgltf_attribute_type_normal:
            mesh->normalBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") normal", true));
            break;
        case cgltf_attribute_type_tangent:
            mesh->tangentBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") tangent", false));
            break;
        case cgltf_attribute_type_joints:
            mesh->boneIDBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") boneID", false));
            break;
        case cgltf_attribute_type_weights:
            mesh->boneWeightBuffer.reset(LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") boneWeight", false));
            break;
        default:
            break;
        }
    }

    return mesh;
}

inline bool TextureExists(const cgltf_texture_view& texture_view)
{
    if (texture_view.texture && texture_view.texture->image->uri)
    {
        return true;
    }
    return false;
}

Model::Material* Model::LoadMaterial(const cgltf_material* gltf_material)
{
    Material* material = new Material;
    if (gltf_material == nullptr)
    {
        return material;
    }

    RE_ASSERT(gltf_material->has_pbr_metallic_roughness);
    material->name = gltf_material->name != nullptr ? gltf_material->name : "";

    if (TextureExists(gltf_material->pbr_metallic_roughness.base_color_texture))
    {
        material->albedoTexture = LoadTexture(gltf_material->pbr_metallic_roughness.base_color_texture.texture->image->uri, true);
    }

    if (TextureExists(gltf_material->pbr_metallic_roughness.metallic_roughness_texture))
    {
        material->metallicRoughnessTexture = LoadTexture(gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri, false);
    }

    if (TextureExists(gltf_material->normal_texture))
    {
        material->normalTexture = LoadTexture(gltf_material->normal_texture.texture->image->uri, false);
    }

    if (TextureExists(gltf_material->emissive_texture))
    {
        material->emissiveTexture = LoadTexture(gltf_material->emissive_texture.texture->image->uri, true);
    }

    if (TextureExists(gltf_material->occlusion_texture))
    {
        material->aoTexture = LoadTexture(gltf_material->occlusion_texture.texture->image->uri, false);
    }

    material->albedoColor = float3(gltf_material->pbr_metallic_roughness.base_color_factor);
    material->emissiveColor = float3(gltf_material->emissive_factor);
    material->metallic = gltf_material->pbr_metallic_roughness.metallic_factor;
    material->roughness = gltf_material->pbr_metallic_roughness.roughness_factor;
    material->alphaCutoff = gltf_material->alpha_cutoff;
    material->alphaTest = gltf_material->alpha_mode == cgltf_alpha_mode_mask;

    return material;
}

IndexBuffer* Model::LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name)
{
    RE_ASSERT(accessor->component_type == cgltf_component_type_r_16u || accessor->component_type == cgltf_component_type_r_32u);

    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t index_count = (uint32_t)accessor->count;
    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IndexBuffer* buffer = pRenderer->CreateIndexBuffer(data, stride, index_count, name);

    return buffer;
}

StructuredBuffer* Model::LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, bool convertToLH)
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

Model::Animation* Model::LoadAnimation(const cgltf_animation* gltf_animation)
{
    Animation* animation = new Animation;

    animation->channels.resize(gltf_animation->channels_count);
    for (cgltf_size i = 0; i < gltf_animation->channels_count; ++i)
    {
        LoadAnimationChannel(&gltf_animation->channels[i], animation->channels[i]);
    }

    for (size_t i = 0; i < animation->channels.size(); ++i)
    {
        for (size_t j = 0; j < animation->channels[i].keyframes.size(); ++j)
        {
            animation->timeDuration = max(animation->channels[i].keyframes[j].first, animation->timeDuration);
        }
    }

    return animation;
}

void Model::LoadAnimationChannel(const cgltf_animation_channel* gltf_channel, AnimationChannel& channel)
{
    auto iter = m_nodeMapping.find(gltf_channel->target_node);
    RE_ASSERT(iter != m_nodeMapping.end());
    channel.targetNode = iter->second;

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

    switch (sampler->interpolation)
    {
    case cgltf_interpolation_type_linear:
        channel.interpolation = AnimationInterpolation::Linear;
        break;
    case cgltf_interpolation_type_step:
        channel.interpolation = AnimationInterpolation::Step;
        break;
    case cgltf_interpolation_type_cubic_spline:
        channel.interpolation = AnimationInterpolation::CubicSpline;
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    cgltf_accessor* time_accessor = sampler->input;
    cgltf_accessor* value_accessor = sampler->output;
    RE_ASSERT(time_accessor->count == value_accessor->count);

    cgltf_size keyframe_num = time_accessor->count;
    channel.keyframes.reserve(keyframe_num);

    char* time_data = (char*)time_accessor->buffer_view->buffer->data + time_accessor->buffer_view->offset + time_accessor->offset;
    char* value_data = (char*)value_accessor->buffer_view->buffer->data + value_accessor->buffer_view->offset + value_accessor->offset;

    for (cgltf_size i = 0; i < keyframe_num; ++i)
    {
        float time;
        float4 value;

        memcpy(&time, time_data + time_accessor->stride * i, time_accessor->stride);
        memcpy(&value, value_data + value_accessor->stride * i, value_accessor->stride);

        channel.keyframes.push_back(std::make_pair(time, value));
    }
}

void Model::LoadSkin(const cgltf_skin* skin)
{
    const cgltf_accessor* accessor = skin->inverse_bind_matrices;
    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t size = stride * (uint32_t)accessor->count;
    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    m_inverseBindMatrices.resize(accessor->count);
    memcpy(m_inverseBindMatrices.data(), data, size);

    for (size_t i = 0; i < m_inverseBindMatrices.size(); ++i)
    {
        m_inverseBindMatrices[i] = RightHandToLeftHand(m_inverseBindMatrices[i]);
    }

    m_boneList.reserve(skin->joints_count);
    for (cgltf_size i = 0; i < skin->joints_count; ++i)
    {
        auto iter = m_nodeMapping.find(skin->joints[i]);
        RE_ASSERT(iter != m_nodeMapping.end());

        iter->second->isBone = true;
        m_boneList.push_back(iter->second);
    }

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    m_pBoneMatrixBuffer.reset(pRenderer->CreateRawBuffer(nullptr, sizeof(float4x4) * (uint32_t)m_boneList.size() * MAX_INFLIGHT_FRAMES,
        "Model::m_pBoneMatrixBuffer", GfxMemoryType::CpuToGpu));

    m_boneMatrixBufferOffset = sizeof(float4x4) * (uint32_t)m_boneList.size() * MAX_INFLIGHT_FRAMES;
}

IGfxPipelineState* Model::GetPSO(Material* material)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (material->metallicRoughnessTexture)
    {
        defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");

        if (material->aoTexture == material->metallicRoughnessTexture)
        {
            defines.push_back("AO_METALLIC_ROUGHNESS_TEXTURE=1");
        }
    }

    if (material->normalTexture)
    {
        defines.push_back("NORMAL_TEXTURE=1");

        if (material->normalTexture->GetTexture()->GetDesc().format == GfxFormat::BC5UNORM)
        {
            defines.push_back("RG_NORMAL_TEXTURE=1");
        }
    }

    if (material->alphaTest) defines.push_back("ALPHA_TEST=1");
    if (material->emissiveTexture) defines.push_back("EMISSIVE_TEXTURE=1");
    if (material->aoTexture) defines.push_back("AO_TEXTURE=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA8SRGB;
    psoDesc.rt_format[1] = GfxFormat::RGBA8UNORM;
    psoDesc.rt_format[2] = GfxFormat::RGBA8SRGB;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model PSO");
}

IGfxPipelineState* Model::GetShadowPSO(Material* material) //todo : merge with model.hlsl
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (material->alphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_shadow.hlsl", "vs_main", "vs_6_6", defines);
    if (material->albedoTexture && material->alphaTest)
    {
        psoDesc.ps = pRenderer->GetShader("model_shadow.hlsl", "ps_main", "ps_6_6", defines);
    }
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.rasterizer_state.depth_bias = 5.0f;
    psoDesc.rasterizer_state.depth_slope_scale = 1.0f;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::LessEqual;
    psoDesc.depthstencil_format = GfxFormat::D16;

    return pRenderer->GetPipelineState(psoDesc, "model shadow PSO");
}

IGfxPipelineState* Model::GetVelocityPSO(Material* material)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    std::vector<std::string> defines;
    if (m_pAnimation) defines.push_back("ANIME_POS=1");
    if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
    if (material->alphaTest) defines.push_back("ALPHA_TEST=1");

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = pRenderer->GetShader("model_velocity.hlsl", "vs_main", "vs_6_6", defines);
    psoDesc.ps = pRenderer->GetShader("model_velocity.hlsl", "ps_main", "ps_6_6", defines);
    psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
    psoDesc.rasterizer_state.front_ccw = true;
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.depthstencil_state.depth_test = true;
    psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
    psoDesc.rt_format[0] = GfxFormat::RGBA16F;
    psoDesc.depthstencil_format = GfxFormat::D32FS8;

    return pRenderer->GetPipelineState(psoDesc, "model velocity PSO");
}
