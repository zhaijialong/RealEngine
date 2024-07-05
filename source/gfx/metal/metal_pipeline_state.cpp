#include "metal_pipeline_state.h"
#include "metal_device.h"
#include "metal_shader.h"
#include "metal_utils.h"
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
    descriptor->setFragmentFunction((MTL::Function*)m_desc.ps->GetHandle());
    
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
    
    MTL::Device* device = (MTL::Device*)m_pDevice->GetHandle();
    
    NS::Error* pError = nullptr;
    m_pPSO = device->newComputePipelineState((MTL::Function*)m_desc.cs->GetHandle(), &pError);

    if(!m_pPSO)
    {
        RE_ERROR("[MetalComputePipelineState] failed to create {} \r\n{}", m_name, pError->localizedDescription()->utf8String());
        pError->release();
        return false;
    }
    
    return true;
}
