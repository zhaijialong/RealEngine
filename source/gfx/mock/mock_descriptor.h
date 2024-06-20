#pragma once

#include "../gfx_descriptor.h"

class MockDevice;

class MockShaderResourceView : public IGfxDescriptor
{
public:
    MockShaderResourceView(MockDevice* pDevice, IGfxResource* pResource, const GfxShaderResourceViewDesc& desc, const eastl::string& name);
    ~MockShaderResourceView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxShaderResourceViewDesc m_desc = {};
};

class MockUnorderedAccessView : public IGfxDescriptor
{
public:
    MockUnorderedAccessView(MockDevice* pDevice, IGfxResource* pResource, const GfxUnorderedAccessViewDesc& desc, const eastl::string& name);
    ~MockUnorderedAccessView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxResource* m_pResource = nullptr;
    GfxUnorderedAccessViewDesc m_desc = {};
};

class MockConstantBufferView : public IGfxDescriptor
{
public:
    MockConstantBufferView(MockDevice* pDevice, IGfxBuffer* buffer, const GfxConstantBufferViewDesc& desc, const eastl::string& name);
    ~MockConstantBufferView();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    IGfxBuffer* m_pBuffer = nullptr;
    GfxConstantBufferViewDesc m_desc = {};
};

class MockSampler : public IGfxDescriptor
{
public:
    MockSampler(MockDevice* pDevice, const GfxSamplerDesc& desc, const eastl::string& name);
    ~MockSampler();

    bool Create();

    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;

private:
    GfxSamplerDesc m_desc;
};