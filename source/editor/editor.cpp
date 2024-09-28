#include "editor.h"
#include "core/engine.h"
#include "renderer/texture_loader.h"
#include "utils/assert.h"
#include "utils/system.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // for dock builder api
#include "ImFileDialog/ImFileDialog.h"
#include "ImGuizmo/ImGuizmo.h"

#include <fstream>

Editor::Editor()
{
    ifd::FileDialog::Instance().CreateTexture = [this](uint8_t* data, int w, int h, char fmt) -> void* 
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        Texture2D* texture = pRenderer->CreateTexture2D(w, h, 1, fmt == 1 ? GfxFormat::RGBA8SRGB : GfxFormat::BGRA8SRGB, 0, "ImFileDialog Icon");
        pRenderer->UploadTexture(texture->GetTexture(), data);

        m_fileDialogIcons.insert(eastl::make_pair(texture->GetSRV(), texture));

        return texture->GetSRV();
    };

    ifd::FileDialog::Instance().DeleteTexture = [this](void* tex) 
    {
        m_pendingDeletions.push_back((IGfxDescriptor*)tex); //should be deleted in next frame
    };

    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    m_pTranslateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/translate.png", true));
    m_pRotateIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/rotate.png", true));
    m_pScaleIcon.reset(pRenderer->CreateTexture2D(asset_path + "ui/scale.png", true));
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
    m_commands.clear();

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureMouse && io.MouseClicked[0])
    {
        ImVec2 mousePos = io.MouseClickedPos[0];

        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        pRenderer->RequestMouseHitTest((uint32_t)mousePos.x, (uint32_t)mousePos.y);
    }

    BuildDockLayout();
    DrawMenu();
    DrawToolBar();
    DrawGizmo();
    DrawFrameStats();

    if (m_bShowRenderer)
    {
        ImGui::Begin("Renderer", &m_bShowRenderer);
        
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        pRenderer->OnGui();

        ImGui::End();
    }
}

void Editor::AddGuiCommand(const eastl::string& window, const eastl::string& section, const eastl::function<void()>& command)
{
    m_commands[window].push_back({ section, command });
}

void Editor::BuildDockLayout()
{
    m_dockSpace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (m_bResetLayout)
    {
        ImGui::DockBuilderRemoveNode(m_dockSpace);
        ImGui::DockBuilderAddNode(m_dockSpace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(m_dockSpace, ImGui::GetMainViewport()->WorkSize);

        m_bResetLayout = false;
    }

    if (ImGui::DockBuilderGetNode(m_dockSpace)->IsLeafNode())
    {
        ImGuiID leftNode, rightNode;
        ImGui::DockBuilderSplitNode(m_dockSpace, ImGuiDir_Right, 0.2f, &rightNode, &leftNode);

        ImGui::DockBuilderDockWindow("Renderer", rightNode);

        ImGui::DockBuilderFinish(m_dockSpace);
    }
}

void Editor::DrawMenu()
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

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

        if (ImGui::BeginMenu("Debug"))
        {
            if (ImGui::MenuItem("VSync", "", &m_bVsync))
            {
                pRenderer->GetSwapchain()->SetVSyncEnabled(m_bVsync);
            }

            if (ImGui::MenuItem("GPU Driven Stats", "", &m_bShowGpuDrivenStats))
            {
                pRenderer->SetGpuDrivenStatsEnabled(m_bShowGpuDrivenStats);
            }

            if (ImGui::MenuItem("Debug View Frustum", "", &m_bViewFrustumLocked))
            {
                Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
                camera->LockViewFrustum(m_bViewFrustumLocked);

                m_lockedViewPos = camera->GetPosition();
                m_lockedViewRotation = camera->GetRotation();
            }

            if (ImGui::MenuItem("Show Meshlets", "", &m_bShowMeshlets))
            {
                pRenderer->SetShowMeshletsEnabled(m_bShowMeshlets);
            }

            bool async_compute = pRenderer->IsAsyncComputeEnabled();
            if (ImGui::MenuItem("Async Compute", "", &async_compute))
            {
                pRenderer->SetAsyncComputeEnabled(async_compute);
            }

            if (ImGui::MenuItem("Reload Shaders"))
            {
                pRenderer->ReloadShaders();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Profiler"))
            {
#if RE_PLATFORM_WINDOWS
                ShellExecute(0, 0, L"http://localhost:1338/", 0, 0, SW_SHOW);
#endif
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

            if (ImGui::MenuItem("Render Graph", ""))
            {
                ShowRenderGraph();
            }

            ImGui::MenuItem("Imgui Demo", "", &m_bShowImguiDemo);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window"))
        {
            ImGui::MenuItem("Inspector", "", &m_bShowInspector);
            ImGui::MenuItem("Settings", "", & m_bShowSettings);
            ImGui::MenuItem("Renderer", "", &m_bShowRenderer);

            m_bResetLayout = ImGui::MenuItem("Reset Layout");

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    if (ifd::FileDialog::Instance().IsDone("SceneOpenDialog"))
    {
        if (ifd::FileDialog::Instance().HasResult())
        {
            eastl::string result = ifd::FileDialog::Instance().GetResult().u8string().c_str();
            Engine::GetInstance()->GetWorld()->LoadScene(result);
        }

        ifd::FileDialog::Instance().Close();
    }

    if (m_bViewFrustumLocked)
    {
        float3 scale = float3(1.0f, 1.0f, 1.0f);
        float4x4 mtxWorld;
        ImGuizmo::RecomposeMatrixFromComponents((const float*)&m_lockedViewPos, (const float*)&m_lockedViewRotation, (const float*)&scale, (float*)&mtxWorld);

        Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
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

    if (m_bShowGpuMemoryStats && m_pGpuMemoryStats)
    {
        ImGui::Begin("GPU Memory Stats", &m_bShowGpuMemoryStats);
        const GfxTextureDesc& desc = m_pGpuMemoryStats->GetTexture()->GetDesc();
        ImGui::Image((ImTextureID)m_pGpuMemoryStats->GetSRV(), ImVec2((float)desc.width, (float)desc.height));
        ImGui::End();
    }

    if (m_bShowImguiDemo)
    {
        ImGui::ShowDemoWindow(&m_bShowImguiDemo);
    }

    if (m_bShowInspector)
    {
        Engine::GetInstance()->GetGUI()->AddCommand([&]() { DrawWindow("Inspector", &m_bShowInspector); });
    }

    if (m_bShowSettings)
    {
        Engine::GetInstance()->GetGUI()->AddCommand([&]() { DrawWindow("Settings", &m_bShowSettings); });
    }
}

void Editor::DrawToolBar()
{
    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(ImVec2(300, 30));

    ImGui::Begin("EditorToolBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));

    ImVec4 focusedBG(1.0f, 0.6f, 0.2f, 0.5f);
    ImVec4 normalBG(0.0f, 0.0f, 0.0f, 0.0f);

    if (ImGui::ImageButton("translate_button##editor", (ImTextureID)m_pTranslateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Translate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Translate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton("rotate_button##editor", (ImTextureID)m_pRotateIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Rotate ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Rotate;
    }
    
    ImGui::SameLine(0.0f, 0.0f);
    if (ImGui::ImageButton("scale_button##editor", (ImTextureID)m_pScaleIcon->GetSRV(), ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), m_selectEditMode == SelectEditMode::Scale ? focusedBG : normalBG))
    {
        m_selectEditMode = SelectEditMode::Scale;
    }
    ImGui::PopStyleVar();

    ImGui::SameLine(0.0f, 20.0f);
    ImGui::PushItemWidth(150.0f);
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
    int renderOutput = (int)pRenderer->GetOutputType();
    ImGui::Combo("##RenderOutput", &renderOutput, 
        "Default\0Diffuse\0Specular(F0)\0World Normal\0Roughness\0Emissive\0Shading Model\0Custom Data\0AO\0Direct Lighting\0Indirect Specular\0Indirect Diffuse\0Path Tracing\0Physics\0\0", (int)RendererOutput::Max);
    pRenderer->SetOutputType((RendererOutput)renderOutput);
    ImGui::PopItemWidth();

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
    float3 rotation = rotation_angles(pSelectedObject->GetRotation());
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
    pSelectedObject->SetRotation(rotation_quat(rotation));
    pSelectedObject->SetScale(scale);

    pSelectedObject->OnGui();
}

void Editor::DrawFrameStats()
{
    ImVec2 windowPos(ImGui::GetIO().DisplaySize.x - 200.0f, 50.0f);

    ImGuiDockNode* dockSapce = ImGui::DockBuilderGetNode(m_dockSpace);
    ImGuiDockNode* centralNode = dockSapce->CentralNode;
    if (centralNode)
    {
        windowPos.x = centralNode->Size.x - 200.0f;
    }
    
    ImGui::SetNextWindowPos(windowPos);
    ImGui::SetNextWindowSize(ImVec2(200.0f, 50.0f));
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
        eastl::string path = pEngine->GetWorkPath();
        eastl::string cmd = "python " + path + "tools/D3d12maDumpVis.py -o " + path + "d3d12ma.png " + path + "d3d12ma.json";

        if (ExecuteCommand(cmd.c_str()) == 0)
        {
            eastl::string file = path + "d3d12ma.png";
            m_pGpuMemoryStats.reset(pRenderer->CreateTexture2D(file, true));
        }
    }
}

void Editor::ShowRenderGraph()
{
    Engine* pEngine = Engine::GetInstance();
    Renderer* pRenderer = pEngine->GetRenderer();

    eastl::string file = pEngine->GetWorkPath() + "tools/graphviz/rendergraph.html";
    eastl::string graph = pRenderer->GetRenderGraph()->Export();
    
    std::ofstream stream;
    stream.open(file.c_str());
    stream << R"(<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Render Graph</title>
  </head>
  <body>
    <script src="viz-standalone.js"></script>
    <script>
        Viz.instance()
            .then(viz => {
                document.body.appendChild(viz.renderSVGElement(`
)";
    stream << graph.c_str();
    stream << R"(
                `));
            })
            .catch(error => {
                console.error(error);
            });
    </script>
  </body>
</html>
)";
    
    stream.close();

    eastl::string command = "start " + file;
    ExecuteCommand(command.c_str());
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

void Editor::DrawWindow(const eastl::string& window, bool* open)
{
    ImGui::Begin(window.c_str(), open);

    auto iter = m_commands.find(window);
    if (iter != m_commands.end())
    {
        for (size_t i = 0; i < iter->second.size(); ++i)
        {
            const Command& cmd = iter->second[i];

            if (ImGui::CollapsingHeader(cmd.section.c_str()))
            {
                cmd.func();
            }
        }
    }

    ImGui::End();
}
