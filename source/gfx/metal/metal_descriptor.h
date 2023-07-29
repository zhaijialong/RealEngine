#pragma once

#include "../gfx_descriptor.h"

class MetalShaderResourceView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MetalUnorderedAccessView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MetalConstantBufferView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MetalSampler : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};