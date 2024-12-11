#include "directional_light.h"
#include "resource_cache.h"
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

    GUI("Settings", "Sun Light", [&]()
        {
            ImGui::DragFloat3("Rotation##DirectionalLight", &m_rotation.x, 1.0f, -180.0f, 180.0f);
            ImGui::DragFloat("Source Radius##DirectionalLight", &m_lightSourceRadius, 0.01f, 0.0f, 0.1f);
            ImGui::DragFloat("Intensity##DirectionalLight", &m_lightIntensity, 0.1f, 0.0f, 100.0f);
            ImGui::ColorEdit3("Color##DirectionalLight", &m_lightColor.x);
        });
}
