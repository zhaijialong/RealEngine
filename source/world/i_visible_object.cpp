#include "i_visible_object.h"
#include "utils/gui_util.h"

void IVisibleObject::OnGui()
{
    GUI("Inspector", "Transform", [&]()
    {
        ImGui::DragFloat3("Position", (float*)&m_pos, 0.01f, -1e8, 1e8, "%.3f");
        ImGui::DragFloat3("Rotation", (float*)&m_rotation, 0.1f, -180.0f, 180.0f, "%.3f");
        ImGui::DragFloat3("Scale", (float*)&m_scale, 0.01f, -1e8, 1.e8, "%.3f");
    });
}
