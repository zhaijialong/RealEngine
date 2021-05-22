#include "render_target.h"
#include "core/engine.h"
#include "renderer.h"

RenderTarget::RenderTarget(bool auto_resize, float size, const std::string& name) : m_resizeConnection({})
{
    m_name = name;
    m_bAutoResize = auto_resize;
    m_size = size;

    if (auto_resize)
    {
        m_resizeConnection = Engine::GetInstance()->WindowResizeSignal.connect(this, &RenderTarget::OnWindowResize);
    }
}

RenderTarget::~RenderTarget()
{
    if (m_bAutoResize)
    {
        Engine::GetInstance()->WindowResizeSignal.disconnect(m_resizeConnection);
    }
}

bool RenderTarget::Create(uint32_t width, uint32_t height, GfxFormat format, GfxTextureUsageFlags flags)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc;
    desc.width = (uint32_t)ceilf(width * m_size);
    desc.height = (uint32_t)ceilf(height * m_size);
    desc.format = format;
    desc.alloc_type = GfxAllocationType::Committed;
    desc.usage = flags;

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));
    if (m_pTexture == nullptr)
    {
        return false;
    }

    if (flags & GfxTextureUsageShaderResource)
    {
        GfxShaderResourceViewDesc srvDesc;
        m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    }

    if (flags & GfxTextureUsageUnorderedAccess)
    {
        //todo : uav
    }

    return true;
}

void RenderTarget::OnWindowResize(uint32_t width, uint32_t height)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    GfxTextureDesc desc = m_pTexture->GetDesc();
    desc.width = (uint32_t)ceilf(width * m_size);
    desc.height = (uint32_t)ceilf(height * m_size);

    m_pTexture.reset(pDevice->CreateTexture(desc, m_name));

    if (m_pSRV)
    {
        GfxShaderResourceViewDesc srvDesc;
        m_pSRV.reset(pDevice->CreateShaderResourceView(m_pTexture.get(), srvDesc, m_name));
    }

    //todo : uav
}
