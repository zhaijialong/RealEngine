#pragma once

#include "core/engine.h"
#include "imgui/imgui.h"
#include "im3d/im3d.h"

inline void GUI(const eastl::string& window, const eastl::string& section, const eastl::function<void()>& command)
{
    Editor* editor = Engine::GetInstance()->GetEditor();
    editor->AddGuiCommand(window, section, command);
}