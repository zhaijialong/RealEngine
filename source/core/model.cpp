#include "model.h"
#include "engine.h"
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
    float4x4 R = rotation_matrix(rotation_quat(degree_to_randian(m_rotation)));
    float4x4 S = scaling_matrix(m_scale);
    float4x4 mtxWorld = mul(T, mul(R, S));

    RenderFunc bassPassBatch = std::bind(&Model::RenderBassPass, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, m_pRootNode.get(), mtxWorld);
    pRenderer->AddBasePassBatch(bassPassBatch);

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

    pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 1, &modelCB, sizeof(modelCB));

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        Material* material = mesh->material.get();

        RENDER_EVENT(pCommandList, m_file + " " + mesh->name);

        std::vector<std::string> defines;
        if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (material->alphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model_shadow.hlsl", "vs_main", "vs_6_6", defines);
        //psoDesc.ps = pRenderer->GetShader("model_shadow.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = true;
        psoDesc.rasterizer_state.depth_bias = 5.0f;
        psoDesc.rasterizer_state.depth_slope_scale = 1.0f;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::LessEqual;
        psoDesc.depthstencil_format = GfxFormat::D16;

        IGfxPipelineState* pPSO = pRenderer->GetPipelineState(psoDesc, "model shadow PSO");
        pCommandList->SetPipelineState(pPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer.get());

        uint32_t vertexCB[4] =
        {
            mesh->posBufferSRV->GetHeapIndex(),
            mesh->uvBufferSRV->GetHeapIndex(),
            material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE,
            pRenderer->GetLinearSampler()->GetHeapIndex()
        };
        pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, vertexCB, sizeof(vertexCB));

        pCommandList->DrawIndexed(mesh->indexCount);
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

    pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 1, &modelCB, sizeof(modelCB));

    for (size_t i = 0; i < pNode->meshes.size(); ++i)
    {
        Mesh* mesh = pNode->meshes[i].get();
        Material* material = mesh->material.get();

        RENDER_EVENT(pCommandList, m_file + " " + mesh->name);

        std::vector<std::string> defines;
        if (material->albedoTexture) defines.push_back("ALBEDO_TEXTURE=1");
        if (material->metallicRoughnessTexture) defines.push_back("METALLIC_ROUGHNESS_TEXTURE=1");
        if (material->normalTexture) defines.push_back("NORMAL_TEXTURE=1");
        if (material->alphaTest) defines.push_back("ALPHA_TEST=1");

        GfxGraphicsPipelineDesc psoDesc;
        psoDesc.vs = pRenderer->GetShader("model.hlsl", "vs_main", "vs_6_6", defines);
        psoDesc.ps = pRenderer->GetShader("model.hlsl", "ps_main", "ps_6_6", defines);
        psoDesc.rasterizer_state.cull_mode = GfxCullMode::Back;
        psoDesc.rasterizer_state.front_ccw = true;
        psoDesc.depthstencil_state.depth_test = true;
        psoDesc.depthstencil_state.depth_func = GfxCompareFunc::GreaterEqual;
        psoDesc.rt_format[0] = GfxFormat::RGBA16F;
        psoDesc.depthstencil_format = GfxFormat::D32FS8;

        IGfxPipelineState* pPSO = pRenderer->GetPipelineState(psoDesc, "model PSO");
        pCommandList->SetPipelineState(pPSO);
        pCommandList->SetIndexBuffer(mesh->indexBuffer.get());

        uint32_t vertexCB[4] = 
        { 
            mesh->posBufferSRV->GetHeapIndex(),
            mesh->uvBufferSRV->GetHeapIndex(),
            mesh->normalBufferSRV->GetHeapIndex(),
            mesh->tangentBufferSRV ? mesh->tangentBufferSRV->GetHeapIndex() : GFX_INVALID_RESOURCE
        };
        pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, vertexCB, sizeof(vertexCB));

        MaterialConstant materialCB;
        materialCB.linearSampler = pRenderer->GetLinearSampler()->GetHeapIndex();
        materialCB.albedoTexture = material->albedoTexture ? material->albedoTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.metallicRoughnessTexture = material->metallicRoughnessTexture ? material->metallicRoughnessTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.normalTexture = material->normalTexture ? material->normalTexture->GetSRV()->GetHeapIndex() : GFX_INVALID_RESOURCE;
        materialCB.albedo = material->albedoColor;
        materialCB.metallic = material->metallic;
        materialCB.roughness = material->roughness;
        materialCB.alphaCutoff = material->alphaCutoff;

        pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 2, &materialCB, sizeof(materialCB));

        pCommandList->DrawIndexed(mesh->indexCount);
    }

    for (size_t i = 0; i < pNode->childNodes.size(); ++i)
    {
        RenderBassPass(pCommandList, pRenderer, pCamera, pNode->childNodes[i].get(), mtxWorld);
    }
}

Texture* Model::LoadTexture(const std::string& file, bool srgb)
{
    auto iter = m_textures.find(file);
    if (iter != m_textures.end())
    {
        return iter->second.get();
    }

    size_t last_slash = m_file.find_last_of('/');
    std::string path = Engine::GetInstance()->GetAssetPath() + m_file.substr(0, last_slash + 1);

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    Texture* texture = pRenderer->CreateTexture(path + file, srgb);

    if (texture != nullptr)
    {
        m_textures.insert(std::make_pair(file, std::unique_ptr<Texture>(texture)));
    }

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
    mesh->indexCount = (uint32_t)gltf_primitive->indices->count;
    
    for (cgltf_size i = 0; i < gltf_primitive->attributes_count; ++i)
    {
        IGfxBuffer* buffer = nullptr;
        IGfxDescriptor* srv = nullptr;

        switch (gltf_primitive->attributes[i].type)
        {
        case cgltf_attribute_type_position:
            LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") pos", true, &buffer, &srv);
            mesh->posBuffer.reset(buffer);
            mesh->posBufferSRV.reset(srv);
            break;
        case cgltf_attribute_type_texcoord:
            if (gltf_primitive->attributes[i].index == 0)
            {
                LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") UV", false, &buffer, &srv);
                mesh->uvBuffer.reset(buffer);
                mesh->uvBufferSRV.reset(srv);
            }
            break;
        case cgltf_attribute_type_normal:
            LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") normal", true, &buffer, &srv);
            mesh->normalBuffer.reset(buffer);
            mesh->normalBufferSRV.reset(srv);
            break;
        case cgltf_attribute_type_tangent:
            LoadVertexBuffer(gltf_primitive->attributes[i].data, "model(" + m_file + " " + mesh->name + ") tangent", true, &buffer, &srv);
            mesh->tangentBuffer.reset(buffer);
            mesh->tangentBufferSRV.reset(srv);
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

    material->albedoColor = float3(gltf_material->pbr_metallic_roughness.base_color_factor);
    material->metallic = gltf_material->pbr_metallic_roughness.metallic_factor;
    material->roughness = gltf_material->pbr_metallic_roughness.roughness_factor;
    material->alphaCutoff = gltf_material->alpha_cutoff;
    material->alphaTest = gltf_material->alpha_mode == cgltf_alpha_mode_mask;

    return material;
}

IGfxBuffer* Model::LoadIndexBuffer(const cgltf_accessor* accessor, const std::string& name)
{
    GfxFormat format = GfxFormat::Unknown;
    if (accessor->component_type == cgltf_component_type_r_32u)
    {
        format = GfxFormat::R32UI;
    }
    else if (accessor->component_type == cgltf_component_type_r_16u)
    {
        format = GfxFormat::R16UI;
    }

    uint32_t stride = (uint32_t)accessor->stride;
    uint32_t size = stride * (uint32_t)accessor->count;
    void* data = (char*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;

    GfxBufferDesc desc;
    desc.stride = stride;
    desc.size = size;
    desc.format = format;

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    IGfxBuffer* buffer = pDevice->CreateBuffer(desc, name);
    pRenderer->UploadBuffer(buffer, data, size);

    return buffer;
}

void Model::LoadVertexBuffer(const cgltf_accessor* accessor, const std::string& name, bool convertToLH, IGfxBuffer** buffer, IGfxDescriptor** descriptor)
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
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxBufferDesc desc;
    desc.stride = stride;
    desc.size = size;
    desc.usage = GfxBufferUsageStructuredBuffer;

    *buffer = pDevice->CreateBuffer(desc, name);
    RE_ASSERT(*buffer != nullptr);

    pRenderer->UploadBuffer(*buffer, data, size);
    RE_FREE(data);

    GfxShaderResourceViewDesc srvDesc;
    srvDesc.type = GfxShaderResourceViewType::StructuredBuffer;
    srvDesc.buffer.size = size;

    *descriptor = pDevice->CreateShaderResourceView(*buffer, srvDesc, name + " SRV");
}
