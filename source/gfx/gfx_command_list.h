#pragma once

#include "gfx_resource.h"

class IGfxFence;
class IGfxBuffer;
class IGfxTexture;
class IGfxHeap;
class IGfxDescriptor;
class IGfxPipelineState;
class IGfxRayTracingBLAS;
class IGfxRayTracingTLAS;

class IGfxCommandList : public IGfxResource
{
public:
    virtual ~IGfxCommandList() {}

    virtual GfxCommandQueue GetQueue() const = 0;

    virtual void ResetAllocator() = 0;
    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual void Wait(IGfxFence* fence, uint64_t value) = 0;
    virtual void Signal(IGfxFence* fence, uint64_t value) = 0;
    virtual void Submit() = 0;
    virtual void ClearState() = 0;

    virtual void BeginProfiling() = 0;
    virtual void EndProfiling() = 0;
    virtual void BeginEvent(const eastl::string& event_name) = 0;
    virtual void EndEvent() = 0;

    virtual void CopyBufferToTexture(IGfxTexture* dst_texture, uint32_t mip_level, uint32_t array_slice, IGfxBuffer* src_buffer, uint32_t offset) = 0;
    virtual void CopyTextureToBuffer(IGfxBuffer* dst_buffer, uint32_t offset, IGfxTexture* src_texture, uint32_t mip_level, uint32_t array_slice) = 0;
    virtual void CopyBuffer(IGfxBuffer* dst, uint32_t dst_offset, IGfxBuffer* src, uint32_t src_offset, uint32_t size) = 0;
    virtual void CopyTexture(IGfxTexture* dst, uint32_t dst_mip, uint32_t dst_array, IGfxTexture* src, uint32_t src_mip, uint32_t src_array) = 0;
    virtual void ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const float* clear_value) = 0;
    virtual void ClearUAV(IGfxResource* resource, IGfxDescriptor* uav, const uint32_t* clear_value) = 0;
    virtual void WriteBuffer(IGfxBuffer* buffer, uint32_t offset, uint32_t data) = 0;
    virtual void UpdateTileMappings(IGfxTexture* texture, IGfxHeap* heap, uint32_t mapping_count, const GfxTileMapping* mappings) = 0;

    virtual void TextureBarrier(IGfxTexture* texture, uint32_t sub_resource, GfxAccessFlags access_before, GfxAccessFlags access_after) = 0;
    virtual void BufferBarrier(IGfxBuffer* buffer, GfxAccessFlags access_before, GfxAccessFlags access_after) = 0;
    virtual void GlobalBarrier(GfxAccessFlags access_before, GfxAccessFlags access_after) = 0;
    virtual void FlushBarriers() = 0;

    virtual void BeginRenderPass(const GfxRenderPassDesc& render_pass) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipelineState(IGfxPipelineState* state) = 0;
    virtual void SetStencilReference(uint8_t stencil) = 0;
    virtual void SetBlendFactor(const float* blend_factor) = 0;
    virtual void SetIndexBuffer(IGfxBuffer* buffer, uint32_t offset, GfxFormat format) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;
    virtual void SetGraphicsConstants(uint32_t slot, const void* data, size_t data_size) = 0;
    virtual void SetComputeConstants(uint32_t slot, const void* data, size_t data_size) = 0;

    virtual void Draw(uint32_t vertex_count, uint32_t instance_count = 1) = 0;
    virtual void DrawIndexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t index_offset = 0) = 0;
    virtual void Dispatch(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) = 0;
    virtual void DispatchMesh(uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z) = 0;

    virtual void DrawIndirect(IGfxBuffer* buffer, uint32_t offset) = 0;
    virtual void DrawIndexedIndirect(IGfxBuffer* buffer, uint32_t offset) = 0;
    virtual void DispatchIndirect(IGfxBuffer* buffer, uint32_t offset) = 0;
    virtual void DispatchMeshIndirect(IGfxBuffer* buffer, uint32_t offset) = 0;

    virtual void MultiDrawIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) = 0;
    virtual void MultiDrawIndexedIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) = 0;
    virtual void MultiDispatchIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) = 0;
    virtual void MultiDispatchMeshIndirect(uint32_t max_count, IGfxBuffer* args_buffer, uint32_t args_buffer_offset, IGfxBuffer* count_buffer, uint32_t count_buffer_offset) = 0;

    virtual void BuildRayTracingBLAS(IGfxRayTracingBLAS* blas) = 0;
    virtual void UpdateRayTracingBLAS(IGfxRayTracingBLAS* blas, IGfxBuffer* vertex_buffer, uint32_t vertex_buffer_offset) = 0;
    virtual void BuildRayTracingTLAS(IGfxRayTracingTLAS* tlas, const GfxRayTracingInstance* instances, uint32_t instance_count) = 0;

#if MICROPROFILE_GPU_TIMERS
    virtual struct MicroProfileThreadLogGpu* GetProfileLog() const = 0;
#endif
};