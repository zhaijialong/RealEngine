#include "billboard_sprite.h"

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
    desc.rt_format[0] = GfxFormat::RGBA16F;
    desc.depthstencil_format = GfxFormat::D32F;

    m_pSpritePSO = pRenderer->GetPipelineState(desc, "Billboard Sprite PSO");
}

BillboardSpriteRenderer::~BillboardSpriteRenderer()
{
}

void BillboardSpriteRenderer::AddSprite(const float3& position, float size, Texture2D* texture, const float3& color)
{
    Sprite sprite;
    sprite.position = position;
    sprite.size = size;
    sprite.color = color;
    sprite.texture = texture->GetSRV()->GetHeapIndex();

    m_sprites.push_back(sprite);
}

void BillboardSpriteRenderer::Render()
{  
    if (m_sprites.empty())
    {
        return;
    }

    uint32_t spriteCount = (uint32_t)m_sprites.size();
    uint32_t spriteBufferAddress = m_pRenderer->AllocateSceneConstant(m_sprites.data(), sizeof(Sprite) * spriteCount);
    m_sprites.clear();

    RenderBatch& batch = m_pRenderer->AddForwardPassBatch();
    batch.label = "BillboardSprite";
    batch.SetPipelineState(m_pSpritePSO);

    uint32_t cb[] = { spriteCount, spriteBufferAddress };
    batch.SetConstantBuffer(0, cb, sizeof(cb));
    batch.DispatchMesh(DivideRoudingUp(spriteCount, 32), 1, 1);
}
