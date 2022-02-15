#include "camera.h"
#include "core/engine.h"
#include "utils/gui_util.h"

float2 CalcDepthlinearizationParams(const float4x4& mtxProjection)
{
    // | 1  0  0  0 |
    // | 0  1  0  0 |
    // | 0  0  A  1 |
    // | 0  0  B  0 |

    // Z' = (Z * A + B) / Z     --- perspective divide
    // Z' = A + B / Z

    // Z = B / (Z' - A)
    // Z = 1 / (Z' * C1 - C2)   --- C1 = 1/B, C2 = A/B

    float A = mtxProjection[2][2];
    float B = max(mtxProjection[3][2], 0.00000001f); //avoid dividing by 0

    float C1 = 1.0f / B;
    float C2 = A / B;

    return float2(C1, C2);
}

Camera::Camera() : 
    m_resizeConnection({})
{
    m_pos = float3(0.0f, 0.0f, 0.0f);
    m_rotation = float3(0.0, 0.0, 0.0f);

    m_resizeConnection = Engine::GetInstance()->WindowResizeSignal.connect(this, &Camera::OnWindowResize);
}

Camera::~Camera()
{
    Engine::GetInstance()->WindowResizeSignal.disconnect(m_resizeConnection);
}

void Camera::SetPerpective(float aspectRatio, float yfov, float znear, float zfar)
{
    m_aspectRatio = aspectRatio;
    m_fov = yfov;
    m_znear = znear;
    m_zfar = zfar;

    m_projection = perspective_matrix(degree_to_randian(yfov), aspectRatio, zfar, znear, pos_z, zero_to_one);
}

void Camera::SetPosition(const float3& pos)
{
    m_pos = pos;
}

void Camera::SetRotation(const float3& rotation)
{
    m_rotation = rotation;
}

void Camera::SetFov(float fov)
{
    m_fov = fov;

    SetPerpective(m_aspectRatio, m_fov, m_znear, m_zfar);
}

void Camera::Tick(float delta_time)
{
    GUI("Settings", "Camera", [&]() { OnGui(); });

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard)
    {
        if (ImGui::IsKeyDown('A'))
        {
            m_pos += GetLeft() * m_moveSpeed * delta_time;
        }
        else if (ImGui::IsKeyDown('S'))
        {
            m_pos += GetBack() * m_moveSpeed * delta_time;
        }
        else if (ImGui::IsKeyDown('D'))
        {
            m_pos += GetRight() * m_moveSpeed * delta_time;
        }
        else if (ImGui::IsKeyDown('W'))
        {
            m_pos += GetForward() * m_moveSpeed * delta_time;
        }
    }

    if (!io.WantCaptureMouse)
    {
        m_pos += GetForward() * io.MouseWheel * m_moveSpeed * delta_time;

        if (ImGui::IsMouseDragging(1))
        {
            const float rotate_speed = 0.1f;

            m_rotation.x = fmodf(m_rotation.x + io.MouseDelta.y * rotate_speed, 360.0f);
            m_rotation.y = fmodf(m_rotation.y + io.MouseDelta.x * rotate_speed, 360.0f);
        }
    }

    UpdateJitter();
    UpdateMatrix();

    if (!m_bFrustumLocked)
    {
        m_frustumViewPos = m_pos;

        UpdateFrustumPlanes(m_viewProjectionJitter);
    }
}

void Camera::SetupCameraCB(IGfxCommandList* pCommandList)
{
    m_cameraCB.cameraPos = GetPosition();
    m_cameraCB.nearZ = m_znear;
    m_cameraCB.farZ = m_zfar;
    m_cameraCB.linearZParams = CalcDepthlinearizationParams(m_projection);
    m_cameraCB.jitter = m_jitter;
    m_cameraCB.prevJitter = m_prevJitter;
    
    m_cameraCB.mtxView = m_view;
    m_cameraCB.mtxViewInverse = inverse(m_view);
    m_cameraCB.mtxProjection = m_projectionJitter;
    m_cameraCB.mtxProjectionInverse = inverse(m_projectionJitter);
    m_cameraCB.mtxViewProjection = m_viewProjectionJitter;
    m_cameraCB.mtxViewProjectionInverse = inverse(m_viewProjectionJitter);

    m_cameraCB.mtxViewProjectionNoJitter = m_viewProjection;
    m_cameraCB.mtxPrevViewProjectionNoJitter = m_prevViewProjection;
    m_cameraCB.mtxClipToPrevClipNoJitter = mul(m_prevViewProjection, inverse(m_viewProjection));

    m_cameraCB.mtxPrevViewProjectionInverse = inverse(m_prevViewProjectionJitter);

    m_cameraCB.culling.viewPos = m_frustumViewPos;

    m_cameraCB.physicalCamera.aperture = m_aperture;
    m_cameraCB.physicalCamera.shutterSpeed = 1.0f / m_shutterSpeed;
    m_cameraCB.physicalCamera.iso = m_iso;
    m_cameraCB.physicalCamera.exposureCompensation = m_exposureCompensation;

    for (int i = 0; i < 6; ++i)
    {
        m_cameraCB.culling.planes[i] = m_frustumPlanes[i];
    }

    pCommandList->SetGraphicsConstants(3, &m_cameraCB, sizeof(CameraConstant));
    pCommandList->SetComputeConstants(3, &m_cameraCB, sizeof(CameraConstant));
}

void Camera::UpdateJitter()
{
    if (m_bEnableJitter)
    {
        uint64_t frameIndex = Engine::GetInstance()->GetRenderer()->GetFrameID();

        const float* Offset = nullptr;
        float Scale = 1.0f;
        // following work of Vaidyanathan et all: https://software.intel.com/content/www/us/en/develop/articles/coarse-pixel-shading-with-temporal-supersampling.html
        static const float Halton23_16[16][2] = { { 0.0f, 0.0f }, { 0.5f, 0.333333f }, { 0.25f, 0.666667f }, { 0.75f, 0.111111f }, { 0.125f, 0.444444f }, { 0.625f, 0.777778f }, { 0.375f ,0.222222f }, { 0.875f ,0.555556f }, { 0.0625f, 0.888889f }, { 0.562500f, 0.037037f }, { 0.3125f, 0.37037f }, { 0.8125f, 0.703704f }, { 0.1875f, 0.148148f }, { 0.6875f, 0.481481f }, { 0.4375f ,0.814815f }, { 0.9375f ,0.259259f } };
        static const float BlueNoise_16[16][2] = { { 1.5f, 0.59375f }, { 1.21875f, 1.375f }, { 1.6875f, 1.90625f }, { 0.375f, 0.84375f }, { 1.125f, 1.875f }, { 0.71875f, 1.65625f }, { 1.9375f ,0.71875f }, { 0.65625f ,0.125f }, { 0.90625f, 0.9375f }, { 1.65625f, 1.4375f }, { 0.5f, 1.28125f }, { 0.21875f, 0.0625f }, { 1.843750f, 0.312500f }, { 1.09375f, 0.5625f }, { 0.0625f, 1.21875f }, { 0.28125f, 1.65625f } };

#if 1 // halton
        Offset = Halton23_16[frameIndex % 16];
        static float2 correctionOffset = { -55,0 };
        if (correctionOffset.x == -55)
        {
            correctionOffset = { 0,0 };
            for (int i = 0; i < 16; i++)
                correctionOffset += float2(Halton23_16[i][0], Halton23_16[i][1]);
            correctionOffset /= 16.0f;
            correctionOffset = float2(0.5f, 0.5f) - correctionOffset;
        }
#endif

        m_prevJitter = m_jitter;
        m_jitter = { (Offset[0] + correctionOffset.x) * Scale - 0.5f, (Offset[1] + correctionOffset.y) * Scale - 0.5f };
    }
}

void Camera::UpdateMatrix()
{
    m_prevViewProjection = m_viewProjection;
    m_prevViewProjectionJitter = m_viewProjectionJitter;

    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
    m_view = inverse(m_world);
    m_viewProjection = mul(m_projection, m_view);

    if (m_bEnableJitter)
    {
        uint32_t width = Engine::GetInstance()->GetRenderer()->GetBackbufferWidth();
        uint32_t height = Engine::GetInstance()->GetRenderer()->GetBackbufferHeight();

        float4x4 mtxJitter = translation_matrix(float3(2.0f * m_jitter.x / (float)width, -2.0f * m_jitter.y / (float)height, 0.0f));

        m_projectionJitter = mul(mtxJitter, m_projection);
        m_viewProjectionJitter = mul(m_projectionJitter, m_view);
    }
    else
    {
        m_projectionJitter = m_projection;
        m_viewProjectionJitter = m_viewProjection;
    }
}

void Camera::UpdateFrustumPlanes(const float4x4& matrix)
{
    // https://www.gamedevs.org/uploads/fast-extraction-viewing-frustum-planes-from-world-view-projection-matrix.pdf

    // Left clipping plane
    m_frustumPlanes[0].x = matrix[0][3] + matrix[0][0];
    m_frustumPlanes[0].y = matrix[1][3] + matrix[1][0];
    m_frustumPlanes[0].z = matrix[2][3] + matrix[2][0];
    m_frustumPlanes[0].w = matrix[3][3] + matrix[3][0];
    m_frustumPlanes[0] = normalize_plane(m_frustumPlanes[0]);

    // Right clipping plane
    m_frustumPlanes[1].x = matrix[0][3] - matrix[0][0];
    m_frustumPlanes[1].y = matrix[1][3] - matrix[1][0];
    m_frustumPlanes[1].z = matrix[2][3] - matrix[2][0];
    m_frustumPlanes[1].w = matrix[3][3] - matrix[3][0];
    m_frustumPlanes[1] = normalize_plane(m_frustumPlanes[1]);

    // Top clipping plane
    m_frustumPlanes[2].x = matrix[0][3] - matrix[0][1];
    m_frustumPlanes[2].y = matrix[1][3] - matrix[1][1];
    m_frustumPlanes[2].z = matrix[2][3] - matrix[2][1];
    m_frustumPlanes[2].w = matrix[3][3] - matrix[3][1];
    m_frustumPlanes[2] = normalize_plane(m_frustumPlanes[2]);

    // Bottom clipping plane
    m_frustumPlanes[3].x = matrix[0][3] + matrix[0][1];
    m_frustumPlanes[3].y = matrix[1][3] + matrix[1][1];
    m_frustumPlanes[3].z = matrix[2][3] + matrix[2][1];
    m_frustumPlanes[3].w = matrix[3][3] + matrix[3][1];
    m_frustumPlanes[3] = normalize_plane(m_frustumPlanes[3]);

    // far clipping plane (reversed depth)
    m_frustumPlanes[4].x = matrix[0][2];
    m_frustumPlanes[4].y = matrix[1][2];
    m_frustumPlanes[4].z = matrix[2][2];
    m_frustumPlanes[4].w = matrix[3][2];
    m_frustumPlanes[4] = normalize_plane(m_frustumPlanes[4]);

    // near clipping plane (reversed depth)
    m_frustumPlanes[5].x = matrix[0][3] - matrix[0][2];
    m_frustumPlanes[5].y = matrix[1][3] - matrix[1][2];
    m_frustumPlanes[5].z = matrix[2][3] - matrix[2][2];
    m_frustumPlanes[5].w = matrix[3][3] - matrix[3][2];
    m_frustumPlanes[5] = normalize_plane(m_frustumPlanes[5]);
}

void Camera::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
    m_aspectRatio = (float)width / height;

    SetPerpective(m_aspectRatio, m_fov, m_znear, m_zfar);
}

void Camera::OnGui()
{
    ImGui::SliderFloat("Move Speed", &m_moveSpeed, 1.0f, 200.0f, "%.0f");

    bool perspective_updated = false;
    perspective_updated |= ImGui::SliderFloat("Fov", &m_fov, 5.0f, 135.0f, "%.0f");
    perspective_updated |= ImGui::SliderFloat("Near Plane", &m_znear, 0.0001f, 3.0f, "%.4f");
    perspective_updated |= ImGui::SliderFloat("Far Plane", &m_zfar, 10.0f, 5000.0f, "%.2f");

    if (perspective_updated)
    {
        SetPerpective(m_aspectRatio, m_fov, m_znear, m_zfar);
    }

    if (ImGui::TreeNode("Physical Camera"))
    {
        ImGui::SliderFloat("Aperture", &m_aperture, 0.7f, 32.0f, "f/%.1f");
        ImGui::SliderInt("Shutter Speed", &m_shutterSpeed, 1, 4000, "1/%d");
        ImGui::SliderInt("ISO", &m_iso, 100, 2000, "%d");
        ImGui::SliderFloat("Exposure Compensation", &m_exposureCompensation, -5.0f, 5.0f, "%.1f");

        ImGui::TreePop();
    }
}

void Camera::DrawViewFrustum(IGfxCommandList* pCommandList)
{
    if (!m_bFrustumLocked)
    {
        return;
    }

    GPU_EVENT(pCommandList, "Camera::DrawViewFrustum");

    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    GfxComputePipelineDesc psoDesc;
    psoDesc.cs = pRenderer->GetShader("view_frustum.hlsl", "main", "cs_6_6", {});
    IGfxPipelineState* pso = pRenderer->GetPipelineState(psoDesc, "ViewFrustum PSO");

    pCommandList->SetPipelineState(pso);
    pCommandList->Dispatch(1, 1, 1);
}
