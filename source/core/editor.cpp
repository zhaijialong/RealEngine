#include "editor.h"
#include "engine.h"
#include "imgui/imgui.h"

void Editor::Tick()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open Scene"))
            {
                OpenScene();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tool"))
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

    if (m_bShowGpuMemoryStats)
    {
        ImGui::Begin("GPU Memory Stats");
        ImGui::Image((ImTextureID)m_pGpuMemoryStats->GetSRV(), ImVec2(1200, 380));
        ImGui::End();
    }

    if (m_bShowImguiDemo)
    {
        ImGui::ShowDemoWindow();
    }
}

void Editor::OpenScene()
{
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
