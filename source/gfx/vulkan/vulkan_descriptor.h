#pragma once

#include "../gfx_descriptor.h"

class VulkanShaderResourceView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class VulkanUnorderedAccessView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class VulkanConstantBufferView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class VulkanSampler : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};