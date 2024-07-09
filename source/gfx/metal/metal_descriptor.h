#pragma once

#include "metal_utils.h"
#include "../gfx_descriptor.h"
#include "../gfx_buffer.h"

class MetalDevice;

class MetalShaderResourceView : public IGfxDescriptor
{
public:
    MetalShaderResourceView(MetalDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name);
    ~MetalShaderResourceView();

    bool Create();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
    MTL::Texture* m_pTextureView = nullptr;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class MetalUnorderedAccessView : public IGfxDescriptor
{
public:
    MetalUnorderedAccessView(MetalDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name);
    ~MetalUnorderedAccessView();

    bool Create();

    virtual void* GetHandle() const override { return m_pResource->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
    MTL::Texture* m_pTextureView = nullptr;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class MetalConstantBufferView : public IGfxDescriptor
{
public:
    MetalConstantBufferView(MetalDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name);
    ~MetalConstantBufferView();

    bool Create();

    virtual void* GetHandle() const override { return m_pBuffer->GetHandle(); }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};

class MetalSampler : public IGfxDescriptor
{
public:
    MetalSampler(MetalDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name);
    ~MetalSampler();

    bool Create();

    virtual void* GetHandle() const override { return m_pSampler; }
    virtual uint32_t GetHeapIndex() const override { return m_heapIndex; }

private:
    GfxSamplerDesc m_desc;
    MTL::SamplerState* m_pSampler = nullptr;
    uint32_t m_heapIndex = GFX_INVALID_RESOURCE;
};
