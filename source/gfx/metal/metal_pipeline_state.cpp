#include "metal_pipeline_state.h"
#include "metal_device.h"
#include "metal_shader.h"
#include "utils/log.h"
#include "../gfx.h"

MetalGraphicsPipelineState::MetalGraphicsPipelineState(MetalDevice* pDevice, const GfxGraphicsPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Graphics;
}

MetalGraphicsPipelineState::~MetalGraphicsPipelineState()
{
    m_pPSO->release();
    m_pDepthStencilState->release();
}

bool MetalGraphicsPipelineState::Create()
{
    m_pPSO->release();
    m_pDepthStencilState->release();
    
    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction((MTL::Function*)m_desc.vs->GetHandle());
    if(m_desc.ps)
    {
        descriptor->setFragmentFunction((MTL::Function*)m_desc.ps->GetHandle());
    }
    
    for(uint32_t i = 0; i < 8; ++i)
    {
        if (m_desc.rt_format[i] != GfxFormat::Unknown)
        {
            MTL::RenderPipelineColorAttachmentDescriptor* colorAttachment = descriptor->colorAttachments()->object(i);
            colorAttachment->setPixelFormat(ToPixelFormat(m_desc.rt_format[i]));
            colorAttachment->setBlendingEnabled(m_desc.blend_state[i].blend_enable);
            colorAttachment->setSourceRGBBlendFactor(ToBlendFactor(m_desc.blend_state[i].color_src));
            colorAttachment->setDestinationRGBBlendFactor(ToBlendFactor(m_desc.blend_state[i].color_dst));
            colorAttachment->setRgbBlendOperation(ToBlendOperation(m_desc.blend_state[i].color_op));
            colorAttachment->setSourceAlphaBlendFactor(ToBlendFactor(m_desc.blend_state[i].alpha_src));
            colorAttachment->setDestinationAlphaBlendFactor(ToBlendFactor(m_desc.blend_state[i].alpha_dst));
            colorAttachment->setAlphaBlendOperation(ToBlendOperation(m_desc.blend_state[i].alpha_op));
            colorAttachment->setWriteMask(ToColorWriteMask(m_desc.blend_state[i].write_mask));
        }
    }
    
    descriptor->setDepthAttachmentPixelFormat(ToPixelFormat(m_desc.depthstencil_format));
    if(IsStencilFormat(m_desc.depthstencil_format))
    {
        descriptor->setStencilAttachmentPixelFormat(ToPixelFormat(m_desc.depthstencil_format));
    }
    
    descriptor->setInputPrimitiveTopology(ToTopologyClass(m_desc.primitive_type));
    descriptor->setRasterSampleCount(1);
    SetDebugLabel(descriptor, m_name.c_str());
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    NS::Error* pError = nullptr;
    m_pPSO = device->newRenderPipelineState(descriptor, &pError);
    descriptor->release();
    
    if(!m_pPSO)
    {
        RE_ERROR("[MetalGraphicsPipelineState] failed to create {} \r\n{}", m_name, pError->localizedDescription()->utf8String());
        pError->release();
        return false;
    }
    
    MTL::DepthStencilDescriptor* depthStencilDescriptor = ToDepthStencilDescriptor(m_desc.depthstencil_state);
    m_pDepthStencilState = device->newDepthStencilState(depthStencilDescriptor);
    depthStencilDescriptor->release();
    
    return true;
}

MetalMeshShadingPipelineState::MetalMeshShadingPipelineState(MetalDevice* pDevice, const GfxMeshShadingPipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::MeshShading;
}

MetalMeshShadingPipelineState::~MetalMeshShadingPipelineState()
{
    m_pPSO->release();
    m_pDepthStencilState->release();
}

bool MetalMeshShadingPipelineState::Create()
{
    m_pPSO->release();
    m_pDepthStencilState->release();
    
    MTL::MeshRenderPipelineDescriptor* descriptor = MTL::MeshRenderPipelineDescriptor::alloc()->init();
    descriptor->setMeshFunction((MTL::Function*)m_desc.ms->GetHandle());
    if(m_desc.as)
    {
        descriptor->setObjectFunction((MTL::Function*)m_desc.as->GetHandle());
    }
    if(m_desc.ps)
    {
        descriptor->setFragmentFunction((MTL::Function*)m_desc.ps->GetHandle());
    }
    
    for(uint32_t i = 0; i < 8; ++i)
    {
        if (m_desc.rt_format[i] != GfxFormat::Unknown)
        {
            MTL::RenderPipelineColorAttachmentDescriptor* colorAttachment = descriptor->colorAttachments()->object(i);
            colorAttachment->setPixelFormat(ToPixelFormat(m_desc.rt_format[i]));
            colorAttachment->setBlendingEnabled(m_desc.blend_state[i].blend_enable);
            colorAttachment->setSourceRGBBlendFactor(ToBlendFactor(m_desc.blend_state[i].color_src));
            colorAttachment->setDestinationRGBBlendFactor(ToBlendFactor(m_desc.blend_state[i].color_dst));
            colorAttachment->setRgbBlendOperation(ToBlendOperation(m_desc.blend_state[i].color_op));
            colorAttachment->setSourceAlphaBlendFactor(ToBlendFactor(m_desc.blend_state[i].alpha_src));
            colorAttachment->setDestinationAlphaBlendFactor(ToBlendFactor(m_desc.blend_state[i].alpha_dst));
            colorAttachment->setAlphaBlendOperation(ToBlendOperation(m_desc.blend_state[i].alpha_op));
            colorAttachment->setWriteMask(ToColorWriteMask(m_desc.blend_state[i].write_mask));
        }
    }
    
    descriptor->setDepthAttachmentPixelFormat(ToPixelFormat(m_desc.depthstencil_format));
    if(IsStencilFormat(m_desc.depthstencil_format))
    {
        descriptor->setStencilAttachmentPixelFormat(ToPixelFormat(m_desc.depthstencil_format));
    }
    
    descriptor->setRasterSampleCount(1);
    SetDebugLabel(descriptor, m_name.c_str());
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    NS::Error* pError = nullptr;
    m_pPSO = device->newRenderPipelineState(descriptor, MTL::PipelineOptionNone, nullptr, &pError);
    descriptor->release();
    
    if(!m_pPSO)
    {
        RE_ERROR("[MetalMeshShadingPipelineState] failed to create {} \r\n{}", m_name, pError->localizedDescription()->utf8String());
        pError->release();
        return false;
    }
    
    MTL::DepthStencilDescriptor* depthStencilDescriptor = ToDepthStencilDescriptor(m_desc.depthstencil_state);
    m_pDepthStencilState = device->newDepthStencilState(depthStencilDescriptor);
    depthStencilDescriptor->release();
    
    if(m_desc.as)
    {
        const MetalShaderReflection& reflection = ((MetalShader*)m_desc.as)->GetReflection();
        m_threadsPerObjectThreadgroup = MTL::Size::Make(reflection.threadsPerThreadgroup[0], reflection.threadsPerThreadgroup[1], reflection.threadsPerThreadgroup[2]);
    }
    
    const MetalShaderReflection& reflection = ((MetalShader*)m_desc.ms)->GetReflection();
    m_threadsPerMeshThreadgroup = MTL::Size::Make(reflection.threadsPerThreadgroup[0], reflection.threadsPerThreadgroup[1], reflection.threadsPerThreadgroup[2]);
    
    return true;
}

MetalComputePipelineState::MetalComputePipelineState(MetalDevice* pDevice, const GfxComputePipelineDesc& desc, const eastl::string& name)
{
    m_pDevice = pDevice;
    m_name = name;
    m_desc = desc;
    m_type = GfxPipelineType::Compute;
}

MetalComputePipelineState::~MetalComputePipelineState()
{
    m_pPSO->release();
}

bool MetalComputePipelineState::Create()
{
    m_pPSO->release();
    
    MTL::ComputePipelineDescriptor* descriptor = MTL::ComputePipelineDescriptor::alloc()->init();
    descriptor->setComputeFunction((MTL::Function*)m_desc.cs->GetHandle());
    SetDebugLabel(descriptor, m_name.c_str());
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    NS::Error* pError = nullptr;
    m_pPSO = device->newComputePipelineState(descriptor, MTL::PipelineOptionNone, nullptr, &pError);
    descriptor->release();

    if(!m_pPSO)
    {
        RE_ERROR("[MetalComputePipelineState] failed to create {} \r\n{}", m_name, pError->localizedDescription()->utf8String());
        pError->release();
        return false;
    }
    
    const MetalShaderReflection& reflection = ((MetalShader*)m_desc.cs)->GetReflection();
    m_threadsPerThreadgroup = MTL::Size::Make(reflection.threadsPerThreadgroup[0], reflection.threadsPerThreadgroup[1], reflection.threadsPerThreadgroup[2]);
    
    return true;
}
