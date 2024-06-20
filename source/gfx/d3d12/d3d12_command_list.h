#pragma once

#include "d3d12_header.h"
#include "../gfx_command_list.h"

class IGfxDevice;
class IGfxFence;

class D3D12CommandList : public IGfxCommandList
{
public:
    D3D12CommandList(IGfxDevice* pDevice, GfxCommandQueue queue_type, const eastl::string& name);
    ~D3D12CommandList();

    bool Create();

    virtual void* GetHandle() const override { return m_pCommandList; }

    virtual void ResetAllocator() override;
    virtual void Begin() override;
    virtual void End() override;
    virtual void Wait(IGfxFence* fence, uint64_t value) override;
    virtual void Signal(IGfxFence* fence, uint64_t value) override;
    virtual void Present(IGfxSwapchain* swapchain) override;
    virtual void Submit() override;
    virtual void ResetState() override;

    virtual void BeginProfiling() override;
    virtual void EndProfiling() override;
    virtual void BeginEvent(const eastl::string& event_name) override;
    virtual void EndEvent() override;

    virtual void CopyBufferToTexture(IGfxTexture* texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* buffer, uint32_t offset) override;
    virtual void CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice) override;
    virtual void CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size) override;
    virtual void CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array) override;
    virtual void ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value) override;
    virtual void ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value) override;
    virtual void WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data) override;
    virtual void UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings) override;

    virtual void TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after) override;
    virtual void BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after) override;
    virtual void GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after) override;
    virtual void FlushBarriers() override;

    virtual void BeginRenderPass(const GfxRenderPassDesc& render_pass) override;
    virtual void EndRenderPass() override;
    virtual void SetPipelineState(IGfxPipelineState* state) override;
    virtual void SetStencilReference(uint8_t stencil) override;
    virtual void SetBlendFactor(const float* blend_factor) override;
    virtual void SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format) override;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    virtual void SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size) override;
    virtual void SetComputeConstants(uint32_t slot, const void* data, size_t data_size) override;

    virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1) override;
    virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t index_offset = 0) override;
    virtual void Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;
    virtual void DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) override;

    virtual void DrawIndirect(IGfxBuffer* buffer, uint32_t offset) override;
    virtual void DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset) override;
    virtual void DispatchIndirect(IGfxBuffer* buffer, uint32_t offset) override;
    virtual void DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset) override;

    virtual void MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) override;
    virtual void MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) override;
    virtual void MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) override;
    virtual void MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) override;

    virtual void BuildRayTracingBLAS(IGfxRayTracingBLAS* blas) override;
    virtual void UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset) override;
    virtual void BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count) override;

#if MICROPROFILE_GPU_TIMERS
    virtual struct MicroProfileThreadLogGpu* GetProfileLog() const override;
#endif

private:
    void BeginRenderPassEmulation(unsigned int numRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC* renderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* depthStencil);
    void EndRenderPassEmulation();

private:
    ID3D12CommandQueue* m_pCommandQueue = nullptr;
    ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
    ID3D12GraphicsCommandList7* m_pCommandList = nullptr;

    uint32_t m_commandCount = 0;
    IGfxPipelineState* m_pCurrentPSO = nullptr;

    eastl::vector<D3D12_TEXTURE_BARRIER> m_textureBarriers;
    eastl::vector<D3D12_BUFFER_BARRIER> m_bufferBarriers;
    eastl::vector<D3D12_GLOBAL_BARRIER> m_globalBarriers;

    eastl::vector<eastl::pair<IGfxFence*, uint64_t>> m_pendingWaits;
    eastl::vector<eastl::pair<IGfxFence*, uint64_t>> m_pendingSignals;

    eastl::vector<IGfxSwapchain*> m_pendingSwapchain;

#if MICROPROFILE_GPU_TIMERS_D3D12
    struct MicroProfileThreadLogGpu* m_pProfileLog = nullptr;
    int m_nProfileQueue = -1;
#endif
};