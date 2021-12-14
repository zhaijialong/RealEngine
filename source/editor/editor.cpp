#include "editor.h"
#include "core/engine.h"
#include "utils/assert.h"
#include "utils/system.h"
#include "imgui/imgui.h"
#include "ImFileDialog/ImFileDialog.h"
#include "ImGuizmo/ImGuizmo.h"

Editor::Editor()
{
    ifd::FileDialog::Instance().CreateTexture = [this](uint8_t* data, int w, int h, char fmt) -> void* 
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        Texture2D* texture = pRenderer->CreateTexture2D(w, h, 1, fmt == 1 ? GfxFormat::RGBA8SRGB : GfxFormat::BGRA8SRGB, GfxTextureUsageShaderResource, "ImFileDialog Icon");
        pRenderer->UploadTexture(texture->GetTexture(), data);

        m_fileDialogIcons.insert(std::make_pair(texture->GetSRV(), texture));

        return texture->GetSRV();
    };

    ifd::FileDialog::Instance().DeleteTexture = [this](void* tex) 
    {
        m_pendingDeletions.push_back((IGfxDescriptor*)tex); //should be deleted in next frame
    };

    std::string asset_path = Engine::GetInstance()->GetAssetPath();
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    m_pTranslateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/translate.png", true, false));
    m_pRotateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/rotate.png", true, false));
    m_pScaleIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/scale.png", true, false));
}

Editor::~Editor()
{
    for (auto iter = m_fileDialogIcons.begin(); iter != m_fileDialogIcons.end(); ++iter)
    {
        delete iter->first;
        delete iter->second;
    }
}

void Editor::Tick()
{
    FlushPendingTextureDeletions();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseClicked[0])
    {
        ImVec2 mousePos = io.MouseClickedPos[0];

        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        pRenderer->RequestMouseHitTest((uint32_t)mousePos.x, (uint32_t)mousePos.y);
    }

    DrawMenu();
    DrawToolBar();
    DrawGizmo();
    DrawFrameStats();
}

void Editor::DrawMenu()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene"))
            {
                ifd::FileDialog::Instance().Open("SceneOpenDialog", "Open Scene", "XML file (*.xml){.xml},.*");
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Profiler"))
            {
                ShellExecute(0, 0, L"http://localhost:1338/", 0, 0, SW_SHOW);
            }

            if (ImGui::MenuItem("GPU Memory Stats", "", &m_bShowGpuMemoryStats))
            {
                if (m_bShowGpuMemoryStats)
                {
                    CreateGpuMemoryStats();
                }
                else
                {
                    m_pGpuMemoryStats.reset();
                }
            }

            if (ImGui::MenuItem("Render Graph", "", &m_bShowRenderGraph))
            {
                if (m_bShowRenderGraph)
                {
                    CreateRenderGraph();
                }
                else
                {
                    m_pRenderGraph.reset();
                }
            }

            ImGui::MenuItem("Imgui Demo", "", &m_bShowImguiDemo);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (ifd::FileDialog::Instance().IsDone("SceneOpenDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            std::string result = ifd::FileDialog::Instance().GetResult().u8string();
            Engine::GetInstance()->GetWorld()->LoadScene(result);
        }

        ifd::FileDialog::Instance().Close();
    }

    if (m_bShowGpuMemoryStats && m_pGpuMemoryStats)
    {
        ImGui::Begin("GPU Memory Stats", &m_bShowGpuMemoryStats);
        const GfxTextureDesc& desc = m_pGpuMemoryStats->GetTexture()->GetDesc();
        ImGui::Image((ImTextureID)m_pGpuMemoryStats->GetSRV(), ImVec2((float)desc.width, (float)desc.height));
        ImGui::End();
    }

    if (m_bShowRenderGraph && m_pRenderGraph)
    {
        ImGui::Begin("Render Graph", &m_bShowRenderGraph, ImGuiWindowFlags_HorizontalScrollbar);
        const GfxTextureDesc& desc = m_pRenderGraph->GetTexture()->GetDesc();
        ImGui::Image((ImTextureID)m_pRenderGraph->GetSRV(), ImVec2((float)desc.width, (float)desc.height));
        ImGui::End();
    }

    if (m_bShowImguiDemo)
    {
        ImGui::ShowDemoWindow(&m_bShowImguiDemo);
    }
}

void Editor::DrawToolBar()
{
    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 30));

    ImGui::Begin("EditorToolBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

    ImVec4 focusedBG(1.0f, 0.6f, 0.2f, 0.5f);
    ImVec4 normalBG(0.0f, 0.0f, 0.0f, 0.0f);

    if (ImGui::ImageButton((ImTextureID)m_pTranslateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), 0, m_selectEditMode == SelectEditMode::Translate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Translate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton((ImTextureID)m_pRotateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), 0, m_selectEditMode == SelectEditMode::Rotate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Rotate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton((ImTextureID)m_pScaleIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), 0, m_selectEditMode == SelectEditMode::Scale ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Scale;
    }

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Camera Speed");

    Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
    float camera_speed = camera->GetMoveSpeed();
    float fov = camera->GetFov();

    ImGui::SameLine(0.0f, 3.0f);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("##CameraSpeed", &camera_speed, 1.0f, 200.0f, "%.0f");
    camera->SetMoveSpeed(camera_speed);

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("FOV");

    ImGui::SameLine(0.0f, 3.0f);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("##CameraFOV", &fov, 5.0f, 135.0f, "%.0f");
    camera->SetFov(fov);

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Checkbox("View Frustum Locked", &m_bViewFrustumLocked))
    {
        camera->LockViewFrustum(m_bViewFrustumLocked);

        m_lockedViewPos = camera->GetPosition();
        m_lockedViewRotation = camera->GetRotation();
    }

    if (m_bViewFrustumLocked)
    {
        float3 scale = float3(1.0f, 1.0f, 1.0f);
        float4x4 mtxWorld;
        ImGuizmo::RecomposeMatrixFromComponents((const float*)&m_lockedViewPos, (const float*)&m_lockedViewRotation, (const float*)&scale, (float*)&mtxWorld);

        float4x4 view = camera->GetViewMatrix();
        float4x4 proj = camera->GetNonJitterProjectionMatrix();

        ImGuizmo::OPERATION operation;
        switch (m_selectEditMode)
        {
        case Editor::SelectEditMode::Translate:
            operation = ImGuizmo::TRANSLATE;
            break;
        case Editor::SelectEditMode::Rotate:
            operation = ImGuizmo::ROTATE;
            break;
        case Editor::SelectEditMode::Scale:
            operation = ImGuizmo::SCALE;
            break;
        default:
            RE_ASSERT(false);
            break;
        }
        ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, operation, ImGuizmo::WORLD, (float*)&mtxWorld);

        ImGuizmo::DecomposeMatrixToComponents((const float*)&mtxWorld, (float*)&m_lockedViewPos, (float*)&m_lockedViewRotation, (float*)&scale);

        camera->SetFrustumViewPosition(m_lockedViewPos);

        float4x4 mtxViewFrustum = mul(camera->GetProjectionMatrix(), inverse(mtxWorld));
        camera->UpdateFrustumPlanes(mtxViewFrustum);
    }

    ImGui::SameLine(0.0f, 20.0f);
    if (ImGui::Checkbox("VSync", &m_bVsync))
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        pRenderer->GetSwapchain()->SetVSyncEnabled(m_bVsync);
    }

    ImGui::End();
}

void Editor::DrawGizmo()
{
    World* world = Engine::GetInstance()->GetWorld();
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    IVisibleObject* pSelectedObject = world->GetVisibleObject(pRenderer->GetMouseHitObjectID());
    if (pSelectedObject == nullptr)
    {
        return;
    }

    float3 pos = pSelectedObject->GetPosition();
    float3 rotation = pSelectedObject->GetRotation();
    float3 scale = pSelectedObject->GetScale();

    float4x4 mtxWorld;
    ImGuizmo::RecomposeMatrixFromComponents((const float*)&pos, (const float*)&rotation, (const float*)&scale, (float*)&mtxWorld);

    ImGuizmo::OPERATION operation;
    switch (m_selectEditMode)
    {
    case Editor::SelectEditMode::Translate:
        operation = ImGuizmo::TRANSLATE;
        break;
    case Editor::SelectEditMode::Rotate:
        operation = ImGuizmo::ROTATE;
        break;
    case Editor::SelectEditMode::Scale:
        operation = ImGuizmo::SCALE;
        break;
    default:
        RE_ASSERT(false);
        break;
    }

    Camera* pCamera = world->GetCamera();
    float4x4 view = pCamera->GetViewMatrix();
    float4x4 proj = pCamera->GetNonJitterProjectionMatrix();

    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, operation, ImGuizmo::WORLD, (float*)&mtxWorld);

    ImGuizmo::DecomposeMatrixToComponents((const float*)&mtxWorld, (float*)&pos, (float*)&rotation, (float*)&scale);
    pSelectedObject->SetPosition(pos);
    pSelectedObject->SetRotation(rotation);
    pSelectedObject->SetScale(scale);
}

void Editor::DrawFrameStats()
{
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 200.0f, 50.0f));
    ImGui::Begin("Frame Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void Editor::CreateGpuMemoryStats()
{
    Engine* pEngine = Engine::GetInstance();
    Renderer* pRenderer = pEngine->GetRenderer();
    IGfxDevice* pDevice = pRenderer->GetDevice();

    if (pDevice->DumpMemoryStats(pEngine->GetWorkPath() + "d3d12ma.json"))
    {
        std::string path = pEngine->GetWorkPath();
        std::string cmd = "python " + path + "tools/D3d12maDumpVis.py -o " + path + "d3d12ma.png " + path + "d3d12ma.json";

        if (ExecuteCommand(cmd.c_str()) == 0)
        {
            std::string file = path + "d3d12ma.png";
            m_pGpuMemoryStats.reset(pRenderer->CreateTexture2D(file, true, false));
        }
    }
}

void Editor::CreateRenderGraph()
{
    Engine* pEngine = Engine::GetInstance();
    Renderer* pRenderer = pEngine->GetRenderer();

    std::string path = pEngine->GetWorkPath();
    std::string graph_file = path + "rendergraph";

    if (pRenderer->GetRenderGraph()->Export(graph_file))
    {
        std::string dot_exe = Engine::GetInstance()->GetWorkPath() + "tools/graphviz/dot.exe";
        std::string cmd = dot_exe + " -Tpng -O " + graph_file;
        if (ExecuteCommand(cmd.c_str()) == 0)
        {
            std::string png_file = graph_file + ".png";
            m_pRenderGraph.reset(pRenderer->CreateTexture2D(png_file, true, false));
        }
    }
}

void Editor::FlushPendingTextureDeletions()
{
    for (size_t i = 0; i < m_pendingDeletions.size(); ++i)
    {
        IGfxDescriptor* srv = m_pendingDeletions[i];
        auto iter = m_fileDialogIcons.find(srv);
        RE_ASSERT(iter != m_fileDialogIcons.end());

        Texture2D* texture = iter->second;
        m_fileDialogIcons.erase(srv);

        delete texture;
    }

    m_pendingDeletions.clear();
}
