#pragma once

#include "../gfx_descriptor.h"

class MetalDevice;

class MetalShaderResourceView : public IGfxDescriptor
{
public:
    MetalShaderResourceView(MetalDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name);
    ~MetalShaderResourceView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
};

class MetalUnorderedAccessView : public IGfxDescriptor
{
public:
    MetalUnorderedAccessView(MetalDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name);
    ~MetalUnorderedAccessView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
};

class MetalConstantBufferView : public IGfxDescriptor
{
public:
    MetalConstantBufferView(MetalDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name);
    ~MetalConstantBufferView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
};

class MetalSampler : public IGfxDescriptor
{
public:
    MetalSampler(MetalDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name);
    ~MetalSampler();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    GfxSamplerDesc m_desc;
};
