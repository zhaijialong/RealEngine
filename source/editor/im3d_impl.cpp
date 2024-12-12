#include "im3d_impl.h"
#include "im3d/im3d.h"

Im3dImpl::Im3dImpl(Renderer* pRenderer) : m_pRenderer(pRenderer)
{
}

Im3dImpl::~Im3dImpl()
{
}

bool Im3dImpl::Init()
{
    GfxGraphicsPipelineDesc desc;
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

    desc.vs = m_pRenderer->GetShader("im3d.hlsl", "vs_main", GfxShaderType::VS, { "POINTS=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "POINTS=1" });
    desc.primitive_type = GfxPrimitiveType::PointList;
    m_pPointPSO = m_pRenderer->GetPipelineState(desc, "Im3d Points PSO");

    desc.vs = m_pRenderer->GetShader("im3d.hlsl", "vs_main", GfxShaderType::VS, { "LINES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "LINES=1" });
    desc.primitive_type = GfxPrimitiveType::LineList;
    m_pLinePSO = m_pRenderer->GetPipelineState(desc, "Im3d Lines PSO");

    desc.vs = m_pRenderer->GetShader("im3d.hlsl", "vs_main", GfxShaderType::VS, { "TRIANGLES=1" });
    desc.ps = m_pRenderer->GetShader("im3d.hlsl", "ps_main", GfxShaderType::PS, { "TRIANGLES=1" });
    desc.primitive_type = GfxPrimitiveType::TriangleList;
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
    appData.m_projScaleY = tanf(degree_to_radian(camera->GetFov()) * 0.5f) * 2.0f;

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
    appData.m_snapRotation = ctrlDown ? degree_to_radian(30.0f) : 0.0f;
    appData.m_snapScale = ctrlDown ? 0.5f : 0.0f;

    Im3d::NewFrame();
}

void Im3dImpl::Render(IGfxCommandList* pCommandList)
{
    Im3d::EndFrame();

    for (unsigned int i = 0; i < Im3d::GetDrawListCount(); ++i)
    {
        const Im3d::DrawList& drawList = Im3d::GetDrawLists()[i];
    }
}
