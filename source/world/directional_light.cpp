#include "directional_light.h"
#include "resource_cache.h"
#include "core/engine.h"
#include "utils/gui_util.h"

bool DirectionalLight::Create()
{
    eastl::string asset_path = Engine::GetInstance()->GetAssetPath();
    m_pIconTexture = ResourceCache::GetInstance()->GetTexture2D(asset_path + "ui/directional_light.png");

    return true;
}

void DirectionalLight::Tick(float delta_time)
{
    float4x4 R = rotation_matrix(m_rotation);
    m_lightDir = normalize(mul(R, float4(0.0f, 1.0f, 0.0f, 0.0f)).xyz());

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantCaptureKeyboard && !io.WantCaptureMouse && ImGui::IsKeyDown(ImGuiKey_L))
    {
        Camera* camera = Engine::GetInstance()->GetWorld()->GetCamera();
        const float4x4& mtxView = camera->GetViewMatrix();

        m_lightDir = normalize(mul(mtxView, float4(m_lightDir, 0.0f)).xyz());

        static float3 rotation = {};
        rotation.z += io.MouseDelta.x * 0.1f;
        rotation.x += io.MouseDelta.y * 0.1f;

        float4x4 viewSpaceR = rotation_matrix(rotation_quat(rotation));
        m_lightDir = mul(viewSpaceR, float4(m_lightDir, 0.0f)).xyz();

        m_lightDir = normalize(mul(inverse(mtxView), float4(m_lightDir, 0.0)).xyz());
    }
}

void DirectionalLight::OnGui()
{
    IVisibleObject::OnGui();

    if (ImGui::CollapsingHeader("DirectionalLight"))
    {
        ImGui::ColorEdit3("Color##Light", (float*)&m_lightColor, ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
        ImGui::SliderFloat("Intensity##Light", &m_lightIntensity, 0.0f, 100.0f);
        ImGui::SliderFloat("Source Radius##Light", &m_lightSourceRadius, 0.0f, 0.01f);
    }

    float4x4 T = translation_matrix(m_pos);
    float4x4 R = rotation_matrix(m_rotation);

    float length = Im3d::GetContext().pixelsToWorldSize(m_pos, 100.0f);

    Im3d::PushMatrix(mul(T, R));
    Im3d::PushSize(3.0);
    Im3d::DrawArrow(float3(0.0f, 0.0f, 0.0f), float3(0.0f, -length, 0.0f));
    Im3d::PopSize();
    Im3d::PopMatrix();
}
