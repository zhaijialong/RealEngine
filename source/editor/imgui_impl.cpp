#include "imgui_impl.h"
#include "core/engine.h"
#include "core/platform.h"
#include "utils/profiler.h"
#include "imgui/imgui.h"
#if RE_PLATFORM_WINDOWS
#include "imgui/imgui_impl_win32.h"
#elif RE_PLATFORM_MAC
#include "imgui/imgui_impl_osx.h"
#endif
#include "ImGuizmo/ImGuizmo.h"

ImGuiImpl::ImGuiImpl(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // todo : multi-viewports
    
    io.BackendRendererName = "imgui_impl_realengine";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

#if RE_PLATFORM_WINDOWS
    ImGui_ImplWin32_Init(Engine::GetInstance()->GetWindowHandle());
#elif RE_PLATFORM_MAC
    ImGui_ImplOSX_Init(Engine::GetInstance()->GetWindowHandle());
#endif
}

ImGuiImpl::~ImGuiImpl()
{
#if RE_PLATFORM_WINDOWS
    ImGui_ImplWin32_Shutdown();
#elif RE_PLATFORM_MAC
    ImGui_ImplOSX_Shutdown();
#endif
    ImGui::DestroyContext();
}

bool ImGuiImpl::Init()
{
    IGfxDevice* pDevice = m_pRenderer->GetDevice();

#if RE_PLATFORM_WINDOWS
    float scaling = ImGui_ImplWin32_GetDpiScaleForHwnd(Engine::GetInstance()->GetWindowHandle());
#else
    float scaling = 1.0f;
#endif
    ImGui::GetStyle().ScaleAllSizes(scaling);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scaling;

    ImFontConfig fontConfig;
    fontConfig.OversampleH = fontConfig.OversampleV = 3;
    
    eastl::string font_file = Engine::GetInstance()->GetAssetPath() + "fonts/DroidSans.ttf";
    io.Fonts->AddFontFromFileTTF(font_file.c_str(), 13.0f, &fontConfig);

    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    m_pFontTexture.reset(m_pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA8UNORM, 0, "GUI::m_pFontTexture"));
    m_pRenderer->UploadTexture(m_pFontTexture->GetTexture(), pixels);

    io.Fonts->TexID = (ImTextureID)m_pFontTexture->GetSRV();

    GfxGraphicsPipelineDesc psoDesc;
    psoDesc.vs = m_pRenderer->GetShader("imgui.hlsl", "vs_main", GfxShaderType::VS);
    psoDesc.ps = m_pRenderer->GetShader("imgui.hlsl", "ps_main", GfxShaderType::PS);
    psoDesc.depthstencil_state.depth_write = false;
    psoDesc.blend_state[0].blend_enable = true;
    psoDesc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
    psoDesc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
    psoDesc.blend_state[0].alpha_src = GfxBlendFactor::One;
    psoDesc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
    psoDesc.rt_format[0] = m_pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
    psoDesc.depthstencil_format = GfxFormat::D32F;
    m_pPSO = m_pRenderer->GetPipelineState(psoDesc, "GUI PSO");

    return true;
}

void ImGuiImpl::NewFrame()
{
#if RE_PLATFORM_WINDOWS
    ImGui_ImplWin32_NewFrame();
#elif RE_PLATFORM_MAC
    ImGui_ImplOSX_NewFrame(Engine::GetInstance()->GetWindowHandle());
#endif
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void ImGuiImpl::Render(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "ImGui");

    ImGui::Render();

    IGfxDevice* pDevice = m_pRenderer->GetDevice();
    ImDrawData* draw_data = ImGui::GetDrawData();

    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
    {
        return;
    }

    uint32_t frame_index = pDevice->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;

    if (m_pVertexBuffer[frame_index] == nullptr || m_pVertexBuffer[frame_index]->GetBuffer()->GetDesc().size < draw_data->TotalVtxCount * sizeof(ImDrawVert))
    {
        m_pVertexBuffer[frame_index].reset(m_pRenderer->CreateStructuredBuffer(nullptr, sizeof(ImDrawVert), draw_data->TotalVtxCount + 5000, "GUI VB", GfxMemoryType::CpuToGpu));
    }

    if (m_pIndexBuffer[frame_index] == nullptr || m_pIndexBuffer[frame_index]->GetIndexCount() < (uint32_t)draw_data->TotalIdxCount)
    {
        m_pIndexBuffer[frame_index].reset(m_pRenderer->CreateIndexBuffer(nullptr, sizeof(ImDrawIdx), draw_data->TotalIdxCount + 10000, "GUI IB", GfxMemoryType::CpuToGpu));
    }

    ImDrawVert* vtx_dst = (ImDrawVert*)m_pVertexBuffer[frame_index]->GetBuffer()->GetCpuAddress();
    ImDrawIdx* idx_dst = (ImDrawIdx*)m_pIndexBuffer[frame_index]->GetBuffer()->GetCpuAddress();
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;
    }

    SetupRenderStates(pCommandList, frame_index);

    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    
    ImVec2 clip_off = draw_data->DisplayPos;
    ImVec2 clip_scale = draw_data->FramebufferScale;
    uint32_t viewport_width = draw_data->DisplaySize.x * clip_scale.x;
    uint32_t viewport_height = draw_data->DisplaySize.y * clip_scale.y;
    
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    SetupRenderStates(pCommandList, frame_index);
                }
                else
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
            }
            else
            {
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                {
                    continue;
                }

                pCommandList->SetScissorRect(
                    eastl::max(clip_min.x, 0), 
                    eastl::max(clip_min.y, 0), 
                    eastl::clamp((uint32_t)(clip_max.x - clip_min.x), 0u, viewport_width),
                    eastl::clamp((uint32_t)(clip_max.y - clip_min.y), 0u, viewport_height));

                uint32_t resource_ids[4] = {
                    m_pVertexBuffer[frame_index]->GetSRV()->GetHeapIndex(),
                    pcmd->VtxOffset + global_vtx_offset,
                    ((IGfxDescriptor*)pcmd->TextureId)->GetHeapIndex(),
                    m_pRenderer->GetLinearSampler()->GetHeapIndex() };

                pCommandList->SetGraphicsConstants(0, resource_ids, sizeof(resource_ids));

                pCommandList->DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset);
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

void ImGuiImpl::SetupRenderStates(IGfxCommandList* pCommandList, uint32_t frame_index)
{
    ImDrawData* draw_data = ImGui::GetDrawData();

    pCommandList->SetViewport(
        0,
        0,
        (uint32_t)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x),
        (uint32_t)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y));
    pCommandList->SetPipelineState(m_pPSO);
    pCommandList->SetIndexBuffer(m_pIndexBuffer[frame_index]->GetBuffer(), 0, m_pIndexBuffer[frame_index]->GetFormat());

    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    float mvp[4][4] =
    {
        { 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
        { 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
        { 0.0f,         0.0f,           0.5f,       0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
    };

    pCommandList->SetGraphicsConstants(1, mvp, sizeof(mvp));
}
