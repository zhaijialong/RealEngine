#include "model.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "utils/memory.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf/cgltf.h"

#include "model_constants.hlsli"

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

    RE_ASSERT(data->scenes_count == 1 && data->scene->nodes_count == 1);
    m_pRootNode.reset(LoadNode(data->scene->nodes[0]));

    cgltf_free(data);
    return true;
}

void Model::Tick(float delta_time)
{
}

void Model::Render(Renderer* pRenderer)
{
    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(rotation_quat(m_rotation));
    float4x4 S = scaling_matrix(m_scale);
    float4x4 mtxWorld = mul(T, mul(R, S));

    RenderFunc bassPassBatch = std::bind(&Model::RenderBassPass, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, m_pRootNode.get(), mtxWorld);
    pRenderer->AddGBufferPassBatch(bassPassBatch);

    ShadowRenderFunc shadowPassBatch = std::bind(&Model::RenderShadowPass, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, m_pRootNode.get(), mtxWorld);
    pRenderer->AddShadowPassBatch(shadowPassBatch);
}

void Model::RenderShadowPass(IGfxCommandList* pCommandList, Renderer* pRenderer, const float4x4& mtxVP, Node* pNode, const float4x4& parentWorld)
{
    float4x4 mtxWorld = mul(parentWorld, pNode->localToParentMatrix);
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

        IGfxPipelineState* pPSO = pRenderer->GetPipelineState(psoDesc, "model shadow PSO");
        pCommandList->SetPipelineState(pPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer->GetBuffer());

        uint32_t vertexCB[4] =
        {
            mesh->posBuffer->GetSRV()->GetHeapIndex(),
            mesh->uvBuffer->GetSRV()->GetHeapIndex(),
            material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
            pRenderer->GetLinearSampler()->GetHeapIndex()
        };
        pCommandList->SetGraphicsConstants(0, vertexCB, sizeof(vertexCB));

        pCommandList->DrawIndexed(mesh->indexBuffer->GetIndexCount());
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        RenderShadowPass(pCommandList, pRenderer, mtxVP, pNode->childNodes[i].get(), mtxWorld);
    }
}

void Model::RenderBassPass(IGfxCommandList* pCommandList, Renderer* pRenderer, Camera* pCamera, Node* pNode, const float4x4& parentWorld)
{
    float4x4 mtxWorld = mul(parentWorld, pNode->localToParentMatrix);
    float4x4 mtxWVP = mul(pCamera->GetViewProjectionMatrix(), mtxWorld);

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

        std::vector<std::string> defines;
        if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (material->metallicRoughnessTexture) defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");
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
        psoDesc.rt_format[1] = GfxFormat::RGB10A2UNORM;
        psoDesc.rt_format[2] = GfxFormat::RGBA8SRGB;
        psoDesc.depthstencil_format = GfxFormat::D32FS8;

        IGfxPipelineState* pPSO = pRenderer->GetPipelineState(psoDesc, "model PSO");
        pCommandList->SetPipelineState(pPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer->GetBuffer());

        uint32_t vertexCB[4] = 
        { 
            mesh->posBuffer->GetSRV()->GetHeapIndex(),
            mesh->uvBuffer->GetSRV()->GetHeapIndex(),
            mesh->normalBuffer->GetSRV()->GetHeapIndex(),
            mesh->tangentBuffer ? mesh->tangentBuffer->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE
        };
        pCommandList->SetGraphicsConstants(0, vertexCB, sizeof(vertexCB));

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
        RenderBassPass(pCommandList, pRenderer, pCamera, pNode->childNodes[i].get(), mtxWorld);
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
        node->localToParentMatrix.w.z *= -1.0f;
    }
    else
    {
        float4x4 T = translation_matrix(float3(gltf_node->translation[0], gltf_node->translation[1], -gltf_node->translation[2]));
        float4x4 R = rotation_matrix(float4(gltf_node->rotation));
        float4x4 S = scaling_matrix(float3(gltf_node->scale));
        node->localToParentMatrix = mul(T, mul(R, S));
    }

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
        default:
            break;
        }
    }
    return mesh;
}

Model::Material* Model::LoadMaterial(const cgltf_material* gltf_material)
{
    RE_ASSERT(gltf_material->has_pbr_metallic_roughness);

    Material* material = new Material;
    material->name = gltf_material->name != nullptr ? gltf_material->name : "";

    if (gltf_material->pbr_metallic_roughness.base_color_texture.texture)
    {
        material->albedoTexture = LoadTexture(gltf_material->pbr_metallic_roughness.base_color_texture.texture->image->uri, true);
    }

    if (gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture)
    {
        material->metallicRoughnessTexture = LoadTexture(gltf_material->pbr_metallic_roughness.metallic_roughness_texture.texture->image->uri, false);
    }

    if (gltf_material->normal_texture.texture)
    {
        material->normalTexture = LoadTexture(gltf_material->normal_texture.texture->image->uri, false);
    }

    if (gltf_material->emissive_texture.texture)
    {
        material->emissiveTexture = LoadTexture(gltf_material->emissive_texture.texture->image->uri, true);
    }

    if (gltf_material->occlusion_texture.texture)
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
    RE_ASSERT(accessor->component_type == cgltf_component_type_r_32f);

    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t size = stride * (uint32_t)accessor->count;

    void* data = RE_ALLOC(size);
    memcpy(data, (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset, size);

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
