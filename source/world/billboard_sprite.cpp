#include "billboard_sprite.h"
#include "core/engine.h"
#include "EASTL/sort.h"

BillboardSpriteRenderer::BillboardSpriteRenderer(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    GfxMeshShadingPipelineDesc desc;
    desc.ms = pRenderer->GetShader("billboard_sprite.hlsl", "ms_main", GfxShaderType::MS);
    desc.ps = pRenderer->GetShader("billboard_sprite.hlsl", "ps_main", GfxShaderType::PS);
    desc.rasterizer_state.cull_mode = GfxCullMode::None;
    desc.depthstencil_state.depth_write = false;
    desc.depthstencil_state.depth_test = true;
    desc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
    desc.blend_state[0].blend_enable = true;
    desc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
    desc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
    desc.blend_state[0].alpha_src = GfxBlendFactor::One;
    desc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
    desc.rt_format[0] = pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
    desc.depthstencil_format = GfxFormat::D32F;
    m_pSpritePSO = pRenderer->GetPipelineState(desc, "Billboard Sprite PSO");

    desc.ms = pRenderer->GetShader("billboard_sprite.hlsl", "ms_main", GfxShaderType::MS, { "OBJECT_ID_PASS=1"});
    desc.ps = pRenderer->GetShader("billboard_sprite.hlsl", "ps_main", GfxShaderType::PS, { "OBJECT_ID_PASS=1" });
    desc.blend_state[0].blend_enable = false;
    desc.rt_format[0] = GfxFormat::R32UI;
    m_pSpriteObjectIDPSO = pRenderer->GetPipelineState(desc, "Billboard Sprite PSO");
}

BillboardSpriteRenderer::~BillboardSpriteRenderer()
{
}

void BillboardSpriteRenderer::AddSprite(const float3& position, float size, Texture2D* texture, const float4& color, uint32_t objectID)
{
    Sprite sprite = {};
    sprite.position = position;
    sprite.size = size;
    sprite.color = PackRGBA8Unorm(color);
    sprite.texture = texture->GetSRV()->GetHeapIndex();
    sprite.objectID = objectID;

    m_sprites.push_back(sprite);
}

void BillboardSpriteRenderer::Render()
{  
    if (m_sprites.empty())
    {
        return;
    }

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    for (size_t i = 0; i < m_sprites.size(); ++i)
    {
        m_sprites[i].distance = distance(m_sprites[i].position, camera->GetPosition());
    }

    eastl::sort(m_sprites.begin(), m_sprites.end(), [](const Sprite& a, const Sprite& b)
        {
            return a.distance > b.distance;
        });

    uint32_t spriteCount = (uint32_t)m_sprites.size();
    uint32_t spriteBufferAddress = m_pRenderer->AllocateSceneConstant(m_sprites.data(), sizeof(Sprite) * spriteCount);
    m_sprites.clear();

    uint32_t cb[] = { spriteCount, spriteBufferAddress };

    RenderBatch& batch = m_pRenderer->AddGuiPassBatch();
    batch.label = "BillboardSprite";
    batch.SetPipelineState(m_pSpritePSO);
    batch.SetConstantBuffer(0, cb, sizeof(cb));
    batch.DispatchMesh(DivideRoudingUp(spriteCount, 64), 1, 1);

    if (m_pRenderer->IsEnableMouseHitTest())
    {
        RenderBatch& batch = m_pRenderer->AddObjectIDPassBatch();
        batch.label = "BillboardSprite";
        batch.SetPipelineState(m_pSpriteObjectIDPSO);
        batch.SetConstantBuffer(0, cb, sizeof(cb));
        batch.DispatchMesh(DivideRoudingUp(spriteCount, 64), 1, 1);
    }
}
