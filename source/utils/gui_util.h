#pragma once

#include "core/engine.h"
#include "imgui/imgui.h"

inline void GUI(const std::string& window, const std::string& section, const std::function<void()>& command)
{
    Editor* editor = Engine::GetInstance()->GetEditor();
    editor->AddGuiCommand(window, section, command);
}