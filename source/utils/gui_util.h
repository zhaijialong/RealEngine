#pragma once

#include "core/engine.h"
#include "imgui/imgui.h"

inline void GUI(const eastl::string& window, const eastl::string& section, const eastl::function<void()>& command)
{
    Editor* editor = Engine::GetInstance()->GetEditor();
    editor->AddGuiCommand(window, section, command);
}