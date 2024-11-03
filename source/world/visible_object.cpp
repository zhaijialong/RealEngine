#include "visible_object.h"
#include "utils/gui_util.h"

void IVisibleObject::OnGui()
{
    GUI("Inspector", "Transform", [&]()
    {
        ImGui::DragFloat3("Position", (float*)&m_pos, 0.01f, -1e8, 1e8, "%.3f");

        float3 angles = rotation_angles(GetRotation());
        if (ImGui::DragFloat3("Rotation", (float*)&angles, 0.1f, -180.0f, 180.0f, "%.3f"))
        {
            SetRotation(rotation_quat(angles));
        }

        ImGui::DragFloat3("Scale", (float*)&m_scale, 0.01f, -1e8, 1.e8, "%.3f");
    });
}
