#pragma once

#include "../gfx_descriptor.h"

class MockShaderResourceView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MockUnorderedAccessView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MockConstantBufferView : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};

class MockSampler : public IGfxDescriptor
{
public:
    virtual void* GetHandle() const override;
    virtual uint32_t GetHeapIndex() const override;
};