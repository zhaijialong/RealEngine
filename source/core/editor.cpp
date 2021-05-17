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
            if (ImGui::MenuItem("Show GPU Memory Stats"))
            {
                ShowGpuMemoryStats();
            }

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void Editor::OpenScene()
{
}

void Editor::ShowGpuMemoryStats()
{
    Engine* pEngine = Engine::GetInstance();
    IGfxDevice* pDevice = pEngine->GetRenderer()->GetDevice();

    if (pDevice->DumpMemoryStats(pEngine->GetWorkPath() + "d3d12ma.json"))
    {
        std::string path = pEngine->GetWorkPath();
        std::string cmd = "python " + path + "tools/D3d12maDumpVis.py -o " + path + "d3d12ma.png " + path + "d3d12ma.json";
        WinExec(cmd.c_str(), 0);
    }
}
