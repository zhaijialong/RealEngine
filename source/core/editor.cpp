#include "editor.h"
#include "engine.h"
#include "utils/assert.h"
#include "imgui/imgui.h"
#include "ImFileDialog/ImFileDialog.h"
#include "ImGuizmo/ImGuizmo.h"

Editor::Editor()
{
    ifd::FileDialog::Instance().CreateTexture = [this](uint8_t* data, int w, int h, char fmt) -> void* 
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        IGfxDevice* pDevice = pRenderer->GetDevice();

        GfxTextureDesc desc;
        desc.width = w;
        desc.height = h;
        desc.format = fmt == 1 ? GfxFormat::RGBA8SRGB : GfxFormat::BGRA8SRGB;
        IGfxTexture* texture = pDevice->CreateTexture(desc, "ImFileDialog Icon");
        RE_ASSERT(texture != nullptr);

        pRenderer->UploadTexture(texture, data, w * h * 4);

        GfxShaderResourceViewDesc srvDesc;
        srvDesc.type = GfxShaderResourceViewType::Texture2D;
        srvDesc.texture.mip_levels = 1;
        IGfxDescriptor* srv = pDevice->CreateShaderResourceView(texture, srvDesc, "ImFileDialog Icon");

        m_fileDialogIcons.insert(std::make_pair(srv, texture));

        return srv;
    };

    ifd::FileDialog::Instance().DeleteTexture = [this](void* tex) 
    {
        m_pendingDeletions.push_back((IGfxDescriptor*)tex); //should be deleted in next frame
    };
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

    DrawMenu();
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
            if (ImGui::MenuItem("Show GPU Memory Stats", "", &m_bShowGpuMemoryStats))
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

            ImGui::MenuItem("Show Imgui Demo", "", &m_bShowImguiDemo);

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

    if (m_bShowImguiDemo)
    {
        ImGui::ShowDemoWindow(&m_bShowImguiDemo);
    }
}

void Editor::DrawGizmo()
{
    Camera* pCamera = Engine::GetInstance()->GetWorld()->GetCamera();
    float4x4 view = pCamera->GetViewMatrix();
    float4x4 proj = pCamera->GetProjectionMatrix();

    IVisibleObject* pSelectedObject = Engine::GetInstance()->GetWorld()->GetSelectedObject();
    float3 pos = pSelectedObject->GetPosition();
    float3 rotation = pSelectedObject->GetRotation();
    float3 scale = pSelectedObject->GetScale();

    float4x4 mtxWorld;
    ImGuizmo::RecomposeMatrixFromComponents((const float*)&pos, (const float*)&rotation, (const float*)&scale, (float*)&mtxWorld);

    ImGuizmo::Manipulate((const float*)&view, (const float*)&proj, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, (float*)&mtxWorld);

    ImGuizmo::DecomposeMatrixToComponents((const float*)&mtxWorld, (float*)&pos, (float*)&rotation, (float*)&scale);
    pSelectedObject->SetPosition(pos);
    pSelectedObject->SetRotation(rotation);
    pSelectedObject->SetScale(scale);
}

void Editor::DrawFrameStats()
{
    uint32_t window_width = Engine::GetInstance()->GetRenderer()->GetBackbufferWidth();
    uint32_t window_height = Engine::GetInstance()->GetRenderer()->GetBackbufferHeight();

    ImGui::SetNextWindowPos(ImVec2(window_width - 200.0f, 50.0f));
    ImGui::Begin("Frame Stats", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
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

        if (WinExec(cmd.c_str(), 0) > 31) //"If the function succeeds, the return value is greater than 31."
        {
            std::string file = path + "d3d12ma.png";
            m_pGpuMemoryStats.reset(pRenderer->CreateTexture(file));
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

        IGfxTexture* texture = iter->second;
        m_fileDialogIcons.erase(srv);

        delete texture;
        delete srv;
    }

    m_pendingDeletions.clear();
}
