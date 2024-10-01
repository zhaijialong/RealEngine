#include "metalfx_temporal_upscaler.h"
#if RE_PLATFORM_MAC || RE_PLATFORM_IOS

#include "gfx/metal/metal_command_list.h"
#include "../renderer.h"
#include "utils/gui_util.h"

#define MTLFX_PRIVATE_IMPLEMENTATION
#include "MetalFX/MetalFX.hpp"

MetalFXTemporalUpscaler::MetalFXTemporalUpscaler(Renderer* renderer)
{
    m_pRenderer = renderer;
    
    Engine::GetInstance()->WindowResizeSignal.connect(&MetalFXTemporalUpscaler::OnWindowResize, this);
}

MetalFXTemporalUpscaler::~MetalFXTemporalUpscaler()
{
    m_pUpscaler->release();
}

void MetalFXTemporalUpscaler::OnGui()
{
    if (ImGui::CollapsingHeader("MetalFX Temporal Upscaler"))
    {
        if(ImGui::SliderFloat("Upscale Ratio##MetalFX", &m_upscaleRatio, 1.0, 3.0))
        {
            m_needCreateUpscaler = true;
        }
        
        TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
        if (mode == TemporalSuperResolution::MetalFX)
        {
            m_pRenderer->SetTemporalUpscaleRatio(m_upscaleRatio);
        }
    }
}

RGHandle MetalFXTemporalUpscaler::AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    
    struct MetalFXPassData
    {
        RGHandle input;
        RGHandle depth;
        RGHandle velocity;
        RGHandle exposure;
        RGHandle output;
    };
    
    auto metalfx_pass = pRenderGraph->AddPass<MetalFXPassData>("MetalFX", RenderPassType::Compute,
        [&](MetalFXPassData& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.exposure = builder.Read(exposure);

            RGTexture::Desc desc;
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "MetalFX Output");
            data.output = builder.Write(data.output);
        },
        [=](const MetalFXPassData& data, IGfxCommandList* pCommandList)
        {
            if(m_needCreateUpscaler)
            {
                CreateUpscaler(renderWidth, renderHeight, displayWidth, displayHeight);
            }
        
            pCommandList->FlushBarriers();
        
            RGTexture* inputRT = pRenderGraph->GetTexture(data.input);
            RGTexture* depthRT = pRenderGraph->GetTexture(data.depth);
            RGTexture* velocityRT = pRenderGraph->GetTexture(data.velocity);
            RGTexture* exposureRT = pRenderGraph->GetTexture(data.exposure);
            RGTexture* outputRT = pRenderGraph->GetTexture(data.output);

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

            m_pUpscaler->setColorTexture((MTL::Texture*)inputRT->GetTexture()->GetHandle());
            m_pUpscaler->setDepthTexture((MTL::Texture*)depthRT->GetTexture()->GetHandle());
            m_pUpscaler->setMotionTexture((MTL::Texture*)velocityRT->GetTexture()->GetHandle());
            m_pUpscaler->setExposureTexture((MTL::Texture*)exposureRT->GetTexture()->GetHandle());
            m_pUpscaler->setOutputTexture((MTL::Texture*)outputRT->GetTexture()->GetHandle());
            m_pUpscaler->setInputContentWidth(renderWidth);
            m_pUpscaler->setInputContentHeight(renderHeight);
            m_pUpscaler->setJitterOffsetX(camera->GetJitter().x);
            m_pUpscaler->setJitterOffsetY(camera->GetJitter().y);
            m_pUpscaler->setMotionVectorScaleX(-0.5f * (float)renderWidth);
            m_pUpscaler->setMotionVectorScaleY(0.5f * (float)renderHeight);
            m_pUpscaler->setDepthReversed(true);
            
            m_pUpscaler->setFence(((MetalCommandList*)pCommandList)->GetFence());
            m_pUpscaler->encodeToCommandBuffer((MTL::CommandBuffer*)pCommandList->GetHandle());
        
            pCommandList->ResetState();
            m_pRenderer->SetupGlobalConstants(pCommandList);
        });
    
    return metalfx_pass->output;
}

void MetalFXTemporalUpscaler::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    m_needCreateUpscaler = true;
}

void MetalFXTemporalUpscaler::CreateUpscaler(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    m_pRenderer->WaitGpuFinished();
    m_pUpscaler->release();
    
    MTLFX::TemporalScalerDescriptor* descriptor = MTLFX::TemporalScalerDescriptor::alloc()->init();
    descriptor->setColorTextureFormat(MTL::PixelFormatRGBA16Float);
    descriptor->setDepthTextureFormat(MTL::PixelFormatDepth32Float);
    descriptor->setMotionTextureFormat(MTL::PixelFormatRGBA16Float);
    descriptor->setOutputTextureFormat(MTL::PixelFormatRGBA16Float);
    descriptor->setInputWidth(renderWidth);
    descriptor->setInputHeight(renderHeight);
    descriptor->setOutputWidth(displayWidth);
    descriptor->setOutputHeight(displayHeight);
    descriptor->setAutoExposureEnabled(false);
    descriptor->setReactiveMaskTextureEnabled(false);
    
    MTL::Device* device = (MTL::Device*)m_pRenderer->GetDevice()->GetHandle();
    m_pUpscaler = descriptor->newTemporalScaler(device);
    descriptor->release();
    
    m_needCreateUpscaler = false;
}

#endif // RE_PLATFORM_MAC || RE_PLATFORM_IOS
