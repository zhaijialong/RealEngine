#pragma once

#include "../gfx_descriptor.h"

class WebGPUDevice;

class WebGPUShaderResourceView : public IGfxDescriptor
{
public:
    WebGPUShaderResourceView(WebGPUDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name);
    ~WebGPUShaderResourceView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
};

class WebGPUUnorderedAccessView : public IGfxDescriptor
{
public:
    WebGPUUnorderedAccessView(WebGPUDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name);
    ~WebGPUUnorderedAccessView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
};

class WebGPUConstantBufferView : public IGfxDescriptor
{
public:
    WebGPUConstantBufferView(WebGPUDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name);
    ~WebGPUConstantBufferView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
};

class WebGPUSampler : public IGfxDescriptor
{
public:
    WebGPUSampler(WebGPUDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name);
    ~WebGPUSampler();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    GfxSamplerDesc m_desc;
};