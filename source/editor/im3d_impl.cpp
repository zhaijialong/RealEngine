#include "im3d_impl.h"
#include "core/engine.h"
#include "utils/profiler.h"
#include "im3d/im3d.h"
#include "imgui/imgui.h"

Im3dImpl::Im3dImpl(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
}

Im3dImpl::~Im3dImpl()
{
}

bool Im3dImpl::Init()
{
    GfxMeshShadingPipelineDesc desc;
    desc.rasterizer_state.cull_mode = GfxCullMode::None;
    desc.depthstencil_state.depth_write = false;
    desc.depthstencil_state.depth_test = true;
    desc.depthstencil_state.depth_func = GfxCompareFunc::Greater;
    desc.blend_state[0].blend_enable = true;
    desc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
    desc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
    desc.blend_state[0].alpha_src = GfxBlendFactor::One;
    desc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
    desc.rt_format[0] = m_pRenderer->GetSwapchain()->GetDesc().backbuffer_format;
    desc.depthstencil_format = GfxFormat::D32F;

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "POINTS=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "POINTS=1" });
    m_pPointPSO = m_pRenderer->GetPipelineState(desc, "Im3d Points PSO");

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "LINES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "LINES=1" });
    m_pLinePSO = m_pRenderer->GetPipelineState(desc, "Im3d Lines PSO");

    desc.ms = m_pRenderer->GetShader("im3d.hlsl", "ms_main", GfxShaderType::MS, { "TRIANGLES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "TRIANGLES=1" });
    m_pTrianglePSO = m_pRenderer->GetPipelineState(desc, "Im3d Triangles PSO");

    return true;
}

void Im3dImpl::NewFrame()
{
    Im3d::AppData& appData = Im3d::GetAppData();
    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();

    appData.m_deltaTime = Engine::GetInstance()->GetFrameDeltaTime();
    appData.m_viewportSize = Im3d::Vec2((float)m_pRenderer->GetDisplayWidth(), (float)m_pRenderer->GetDisplayHeight());
    appData.m_viewOrigin = camera->GetPosition();
    appData.m_viewDirection = camera->GetForward();
    appData.m_worldUp = Im3d::Vec3(0.0f, 1.0f, 0.0f);
    appData.m_projScaleY = tanf(radians(camera->GetFov()) * 0.5f) * 2.0f;

    Im3d::Vec2 cursorPos(ImGui::GetIO().MousePos.x, ImGui::GetIO().MousePos.y);
    cursorPos.x = (cursorPos.x / appData.m_viewportSize.x) * 2.0f - 1.0f;
    cursorPos.y = (cursorPos.y / appData.m_viewportSize.y) * 2.0f - 1.0f;
    cursorPos.y = -cursorPos.y;

    float3 rayOrigin, rayDirection;
    rayOrigin = appData.m_viewOrigin;
    rayDirection.x = cursorPos.x / camera->GetNonJitterProjectionMatrix()[0][0];
    rayDirection.y = cursorPos.y / camera->GetNonJitterPrevProjectionMatrix()[1][1];
    rayDirection.z = -1.0f;
    rayDirection = mul(camera->GetWorldMatrix(), float4(normalize(rayDirection), 0.0f)).xyz();

    appData.m_cursorRayOrigin = rayOrigin;
    appData.m_cursorRayDirection = rayDirection;

    bool ctrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
    appData.m_keyDown[Im3d::Key_L] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_L);
    appData.m_keyDown[Im3d::Key_T] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_T);
    appData.m_keyDown[Im3d::Key_R] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_R);
    appData.m_keyDown[Im3d::Key_S] = ctrlDown && ImGui::IsKeyDown(ImGuiKey_S);
    appData.m_keyDown[Im3d::Mouse_Left] = ImGui::IsKeyDown(ImGuiKey_MouseLeft);

    appData.m_snapTranslation = ctrlDown ? 0.1f : 0.0f;
    appData.m_snapRotation = ctrlDown ? radians(30.0f) : 0.0f;
    appData.m_snapScale = ctrlDown ? 0.5f : 0.0f;

    Im3d::NewFrame();
}

void Im3dImpl::Render(IGfxCommandList* pCommandList)
{
    GPU_EVENT(pCommandList, "Im3d");

    Im3d::EndFrame();

    uint32_t frameIndex = m_pRenderer->GetFrameID() % GFX_MAX_INFLIGHT_FRAMES;
    uint vertexCount = GetVertexCount();

    if (m_pVertexBuffer[frameIndex] == nullptr || m_pVertexBuffer[frameIndex]->GetBuffer()->GetDesc().size < vertexCount * sizeof(Im3d::VertexData))
    {
        m_pVertexBuffer[frameIndex].reset(m_pRenderer->CreateRawBuffer(nullptr, (vertexCount + 1000) * sizeof(Im3d::VertexData), "Im3d VB", GfxMemoryType::CpuToGpu));
    }

    uint32_t vertexBufferOffset = 0;

    for (unsigned int i = 0; i < Im3d::GetDrawListCount(); ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];

        memcpy((char*)m_pVertexBuffer[frameIndex]->GetBuffer()->GetCpuAddress() + vertexBufferOffset,
            drawList.m_vertexData, sizeof(Im3d::VertexData) * drawList.m_vertexCount);

        uint32_t primitveCount = 0;

        switch (drawList.m_primType)
        {
        case Im3d::DrawPrimitive_Points:
            primitveCount = drawList.m_vertexCount;
            pCommandList->SetPipelineState(m_pPointPSO);
            break;
        case Im3d::DrawPrimitive_Lines:
            primitveCount = drawList.m_vertexCount / 2;
            pCommandList->SetPipelineState(m_pLinePSO);
            break;
        case Im3d::DrawPrimitive_Triangles:
            primitveCount = drawList.m_vertexCount / 3;
            pCommandList->SetPipelineState(m_pTrianglePSO);
            break;
        default:
            break;
        }

        uint32_t cb[] = 
        {
            primitveCount,
            m_pVertexBuffer[frameIndex]->GetSRV()->GetHeapIndex(),
            vertexBufferOffset,
        };

        pCommandList->SetGraphicsConstants(0, cb, sizeof(cb));
        pCommandList->DispatchMesh(DivideRoudingUp(primitveCount, 64), 1, 1);

        vertexBufferOffset += sizeof(Im3d::VertexData) * drawList.m_vertexCount;
    }
}

uint32_t Im3dImpl::GetVertexCount() const
{
    uint32_t vertexCount = 0;

    for (unsigned int i = 0; i < Im3d::GetDrawListCount(); ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];
        vertexCount += drawList.m_vertexCount;
    }

    return vertexCount;
}
