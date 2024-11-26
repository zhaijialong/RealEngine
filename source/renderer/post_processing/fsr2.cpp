#include "fsr2.h"

#if RE_PLATFORM_WINDOWS

#include "../renderer.h"
#include "gfx/vulkan/vulkan_device.h"
#include "gfx/vulkan/vulkan_descriptor.h"
#include "utils/gui_util.h"
#include "utils/fmt.h"
#include "dx12/ffx_fsr2_dx12.h"
#include "vk/ffx_fsr2_vk.h"

static inline FfxResource GetVulkanResource(FfxFsr2Context* context, RGTexture* texture, const wchar_t* name, FfxResourceStates state = FFX_RESOURCE_STATE_COMPUTE_READ)
{
    VkImage image = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;

    if (texture)
    {
        image = (VkImage)texture->GetTexture()->GetHandle();

        imageView = state == FFX_RESOURCE_STATE_UNORDERED_ACCESS ?
            ((VulkanUnorderedAccessView*)texture->GetUAV())->GetImageView() :
            ((VulkanShaderResourceView*)texture->GetSRV())->GetImageView();

        const GfxTextureDesc& desc = texture->GetTexture()->GetDesc();
        width = desc.width;
        height = desc.height;
        format = ToVulkanFormat(desc.format);
    }

    return ffxGetTextureResourceVK(context, image, imageView, width, height, format, name, state);
}

FSR2::FSR2(Renderer* pRenderer)
{
    m_pRenderer = pRenderer;

    Engine::GetInstance()->WindowResizeSignal.connect(&FSR2::OnWindowResize, this);
}

FSR2::~FSR2()
{
    DestroyFsr2Context();
}

void FSR2::OnGui()
{
    if (ImGui::CollapsingHeader(fmt::format("FSR {}.{}.{}", FFX_FSR2_VERSION_MAJOR, FFX_FSR2_VERSION_MINOR, FFX_FSR2_VERSION_PATCH).c_str()))
    {
        if (ImGui::Combo("Mode##FSR2", (int*)&m_qualityMode, "Custom\0Quality (1.5x)\0Balanced (1.7x)\0Performance (2.0x)\0Ultra Performance (3.0x)\0\0", 5))
        {
            m_needCreateContext = true;
        }

        if (m_qualityMode == 0)
        {
            if (ImGui::SliderFloat("Upscale Ratio##FSR2", &m_customUpscaleRatio, 1.0, 3.0))
            {
                m_needCreateContext = true;
            }
        }

        ImGui::SliderFloat("Sharpness##FSR2", &m_sharpness, 0.0f, 1.0f, "%.2f");

        TemporalSuperResolution mode = m_pRenderer->GetTemporalUpscaleMode();
        if (mode == TemporalSuperResolution::FSR2)
        {
            m_pRenderer->SetTemporalUpscaleRatio(GetUpscaleRatio());
        }
    }
}

RGHandle FSR2::AddPass(RenderGraph* pRenderGraph, RGHandle input, RGHandle depth, RGHandle velocity, RGHandle exposure,
    uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    struct FSR2Data
    {
        RGHandle input;
        RGHandle depth;
        RGHandle velocity;
        RGHandle exposure;
        RGHandle output;
    };

    auto fsr2_pass = pRenderGraph->AddPass<FSR2Data>("FSR 2.0", RenderPassType::Compute,
        [&](FSR2Data& data, RGBuilder& builder)
        {
            data.input = builder.Read(input);
            data.depth = builder.Read(depth);
            data.velocity = builder.Read(velocity);
            data.exposure = builder.Read(exposure);

            RGTexture::Desc desc;
            desc.width = displayWidth;
            desc.height = displayHeight;
            desc.format = GfxFormat::RGBA16F;
            data.output = builder.Create<RGTexture>(desc, "FSR2 Output");
            data.output = builder.Write(data.output);
        },
        [=](const FSR2Data& data, IGfxCommandList* pCommandList)
        {
            if (m_needCreateContext)
            {
                DestroyFsr2Context();
                CreateFsr2Context(renderWidth, renderHeight, displayWidth, displayHeight);
            }

            pCommandList->FlushBarriers();

            RGTexture* inputRT = pRenderGraph->GetTexture(data.input);
            RGTexture* depthRT = pRenderGraph->GetTexture(data.depth);
            RGTexture* velocityRT = pRenderGraph->GetTexture(data.velocity);
            RGTexture* exposureRT = pRenderGraph->GetTexture(data.exposure);
            RGTexture* outputRT = pRenderGraph->GetTexture(data.output);

            Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
            GfxRenderBackend backend = m_pRenderer->GetDevice()->GetDesc().backend;

            FfxFsr2DispatchDescription dispatchDesc = {};

            if (backend == GfxRenderBackend::D3D12)
            {
                dispatchDesc.commandList = ffxGetCommandListDX12((ID3D12CommandList*)pCommandList->GetHandle());
                dispatchDesc.color = ffxGetResourceDX12(&m_context, (ID3D12Resource*)inputRT->GetTexture()->GetHandle(), L"FSR2_InputColor");
                dispatchDesc.depth = ffxGetResourceDX12(&m_context, (ID3D12Resource*)depthRT->GetTexture()->GetHandle(), L"FSR2_InputDepth");
                dispatchDesc.motionVectors = ffxGetResourceDX12(&m_context, (ID3D12Resource*)velocityRT->GetTexture()->GetHandle(), L"FSR2_InputMotionVectors");
                dispatchDesc.exposure = ffxGetResourceDX12(&m_context, (ID3D12Resource*)exposureRT->GetTexture()->GetHandle(), L"FSR2_InputExposure");
                dispatchDesc.reactive = ffxGetResourceDX12(&m_context, nullptr, L"FSR2_InputReactiveMap"); //todo : "applications should write alpha to the reactive mask"
                dispatchDesc.transparencyAndComposition = ffxGetResourceDX12(&m_context, nullptr, L"FSR2_TransparencyAndCompositionMap"); //todo : "raytraced reflections or animated textures"
                dispatchDesc.output = ffxGetResourceDX12(&m_context, (ID3D12Resource*)outputRT->GetTexture()->GetHandle(), L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
            }
            else if (backend == GfxRenderBackend::Vulkan)
            {
                dispatchDesc.commandList = ffxGetCommandListVK((VkCommandBuffer)pCommandList->GetHandle());
                dispatchDesc.color = GetVulkanResource(&m_context, inputRT, L"FSR2_InputColor");
                dispatchDesc.depth = GetVulkanResource(&m_context, depthRT, L"FSR2_InputDepth");
                dispatchDesc.motionVectors = GetVulkanResource(&m_context, velocityRT, L"FSR2_InputMotionVectors");
                dispatchDesc.exposure = GetVulkanResource(&m_context, exposureRT, L"FSR2_InputExposure");
                dispatchDesc.reactive = GetVulkanResource(&m_context, nullptr, L"FSR2_InputReactiveMap"); //todo : "applications should write alpha to the reactive mask"
                dispatchDesc.transparencyAndComposition = GetVulkanResource(&m_context, nullptr, L"FSR2_TransparencyAndCompositionMap"); //todo : "raytraced reflections or animated textures"
                dispatchDesc.output = GetVulkanResource(&m_context, outputRT, L"FSR2_OutputUpscaledColor", FFX_RESOURCE_STATE_UNORDERED_ACCESS);
            }

            dispatchDesc.jitterOffset.x = camera->GetJitter().x;
            dispatchDesc.jitterOffset.y = camera->GetJitter().y;
            dispatchDesc.motionVectorScale.x = -0.5f * (float)renderWidth;
            dispatchDesc.motionVectorScale.y = 0.5f * (float)renderHeight;
            dispatchDesc.reset = false;
            dispatchDesc.enableSharpening = true;
            dispatchDesc.sharpness = m_sharpness;
            dispatchDesc.frameTimeDelta = Engine::GetInstance()->GetFrameDeltaTime() * 1000.0f;
            dispatchDesc.preExposure = 1.0f;
            dispatchDesc.renderSize.width = renderWidth;
            dispatchDesc.renderSize.height = renderHeight;
            //dispatchDesc.cameraFar = camera->GetZFar();
            dispatchDesc.cameraNear = camera->GetZNear();
            dispatchDesc.cameraFovAngleVertical = degree_to_radian(camera->GetFov());

            FfxErrorCode errorCode = ffxFsr2ContextDispatch(&m_context, &dispatchDesc);
            FFX_ASSERT(errorCode == FFX_OK);

            pCommandList->ResetState();
            m_pRenderer->SetupGlobalConstants(pCommandList);
        });

    return fsr2_pass->output;
}

float FSR2::GetUpscaleRatio() const
{
    if (m_qualityMode == 0)
    {
        return m_customUpscaleRatio;
    }
    return ffxFsr2GetUpscaleRatioFromQualityMode(m_qualityMode);
}

void FSR2::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    if (m_desc.displaySize.width != width ||
        m_desc.displaySize.height != height)
    {
        m_needCreateContext = true;
    }
}

void FSR2::CreateFsr2Context(uint32_t renderWidth, uint32_t renderHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    if (m_needCreateContext)
    {
        GfxRenderBackend backend = m_pRenderer->GetDevice()->GetDesc().backend;
        if (backend == GfxRenderBackend::D3D12)
        {
            ID3D12Device* device = (ID3D12Device*)m_pRenderer->GetDevice()->GetHandle();

            const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeDX12();
            void* scratchBuffer = RE_ALLOC(scratchBufferSize);
            FfxErrorCode errorCode = ffxFsr2GetInterfaceDX12(&m_desc.callbacks, device, scratchBuffer, scratchBufferSize);
            FFX_ASSERT(errorCode == FFX_OK);

            m_desc.device = ffxGetDeviceDX12(device);
        }
        else if (backend == GfxRenderBackend::Vulkan)
        {
            VulkanDevice* device = (VulkanDevice*)m_pRenderer->GetDevice();

            const size_t scratchBufferSize = ffxFsr2GetScratchMemorySizeVK(device->GetPhysicalDevice(), vkEnumerateDeviceExtensionProperties);
            void* scratchBuffer = RE_ALLOC(scratchBufferSize);
            FfxErrorCode errorCode = ffxFsr2GetInterfaceVK(&m_desc.callbacks, scratchBuffer, scratchBufferSize, 
                device->GetInstance(), device->GetPhysicalDevice(), vkGetInstanceProcAddr, vkGetDeviceProcAddr);
            FFX_ASSERT(errorCode == FFX_OK);

            m_desc.device = ffxGetDeviceVK(device->GetDevice());
        }
        else
        {
            RE_ASSERT(false);
        }

        m_desc.maxRenderSize.width = renderWidth;
        m_desc.maxRenderSize.height = renderHeight;
        m_desc.displaySize.width = displayWidth;
        m_desc.displaySize.height = displayHeight;
        m_desc.flags = FFX_FSR2_ENABLE_HIGH_DYNAMIC_RANGE | FFX_FSR2_ENABLE_DEPTH_INVERTED | FFX_FSR2_ENABLE_DEPTH_INFINITE;

        FfxErrorCode errorCode = ffxFsr2ContextCreate(&m_context, &m_desc);
        FFX_ASSERT(errorCode == FFX_OK);

        m_needCreateContext = false;
    }
}

void FSR2::DestroyFsr2Context()
{
    if (m_desc.callbacks.scratchBuffer)
    {
        m_pRenderer->WaitGpuFinished();

        ffxFsr2ContextDestroy(&m_context);

        RE_FREE(m_desc.callbacks.scratchBuffer);
        m_desc.callbacks.scratchBuffer = nullptr;
    }
}

#endif // #if RE_PLATFORM_WINDOWS
