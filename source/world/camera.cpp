#include "camera.h"
#include "core/engine.h"
#include "utils/gui_util.h"

static inline float2 CalcDepthLinearizationParams(const float4x4& mtxProjection)
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

Camera::Camera()
{
    m_pos = float3(0.0f, 0.0f, 0.0f);
    m_rotation = float3(0.0, 0.0, 0.0f);

    Engine::GetInstance()->WindowResizeSignal.connect(&Camera::OnWindowResize, this);
}

Camera::~Camera()
{
    Engine::GetInstance()->WindowResizeSignal.disconnect(this);
}

void Camera::SetPerpective(float aspectRatio, float yfov, float znear)
{
    m_aspectRatio = aspectRatio;
    m_fov = yfov;
    m_znear = znear;

    //m_projection = perspective_matrix(degree_to_radian(yfov), aspectRatio, zfar, znear, pos_z, zero_to_one);

    //LH + reversed z + infinite far plane
    float h = 1.0 / std::tan(0.5f * degree_to_radian(yfov));
    float w = h / aspectRatio;
    m_projection = float4x4(0.0);
    m_projection[0][0] = w;
    m_projection[1][1] = h;
    m_projection[2][2] = 0.0f;
    m_projection[2][3] = 1.0f;
    m_projection[3][2] = znear;
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

    SetPerpective(m_aspectRatio, m_fov, m_znear);
}

void Camera::EnableJitter(bool value)
{
    if (m_bEnableJitter != value)
    {
        m_bEnableJitter = value;

        UpdateJitter();
        UpdateMatrix();
    }
}

void Camera::Tick(float delta_time)
{
    GUI("Settings", "Camera", [&]() { OnCameraSettingGui(); });

    m_bMoved = false;

    UpdateCameraRotation(delta_time);
    UpdateCameraPosition(delta_time);

    UpdateJitter();
    UpdateMatrix();

    if (!m_bFrustumLocked)
    {
        m_frustumViewPos = m_pos;

        UpdateFrustumPlanes(m_viewProjectionJitter);
    }
}

void Camera::UpdateCameraRotation(float delta_time)
{
    ImGuiIO& io = ImGui::GetIO();

    if (!io.NavActive)
    {
        const float rotate_speed = 120.0f;

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickRight))
        {
            m_rotation.y = fmodf(m_rotation.y + delta_time * rotate_speed, 360.0f);
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickLeft))
        {
            m_rotation.y = fmodf(m_rotation.y - delta_time * rotate_speed, 360.0f);
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickDown))
        {
            m_rotation.x = fmodf(m_rotation.x + delta_time * rotate_speed, 360.0f);
        }

        if (ImGui::IsKeyDown(ImGuiKey_GamepadRStickUp))
        {
            m_rotation.x = fmodf(m_rotation.x - delta_time * rotate_speed, 360.0f);
        }
    }

    if (!io.WantCaptureMouse)
    {
        if (ImGui::IsMouseDragging(1) && !ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        {
            const float rotate_speed = 0.1f;

            m_rotation.x = fmodf(m_rotation.x + io.MouseDelta.y * rotate_speed, 360.0f);
            m_rotation.y = fmodf(m_rotation.y + io.MouseDelta.x * rotate_speed, 360.0f);

            m_bMoved = true;
        }
    }

    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
}

void Camera::UpdateCameraPosition(float delta_time)
{
    bool moveLeft = false;
    bool moveRight = false;
    bool moveForward = false;
    bool moveBack = false;
    float moveSpeed = m_moveSpeed;

    ImGuiIO& io = ImGui::GetIO();

    if (!io.WantCaptureKeyboard && !io.NavActive)
    {
        moveLeft = ImGui::IsKeyDown(ImGuiKey_A) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickLeft);
        moveBack = ImGui::IsKeyDown(ImGuiKey_S) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickDown);
        moveRight = ImGui::IsKeyDown(ImGuiKey_D) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickRight);
        moveForward = ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_GamepadLStickUp);
    }

    if (!io.WantCaptureMouse)
    {
        if (!nearly_equal(io.MouseWheel, 0.0f))
        {
            moveForward |= io.MouseWheel > 0.0f;
            moveBack |= io.MouseWheel < 0.0f;

            moveSpeed *= abs(io.MouseWheel);
        }

        if (ImGui::IsMouseDragging(1) && ImGui::IsKeyDown(ImGuiKey_LeftAlt))
        {
            moveForward |= io.MouseDelta.y > 0.0f;
            moveBack |= io.MouseDelta.y < 0.0f;

            moveSpeed *= abs(io.MouseDelta.y) * 0.1f;
        }
    }

    float3 moveVelocity = float3(0.0f, 0.0f, 0.0f);
    if (moveForward) moveVelocity += GetForward();
    if (moveBack) moveVelocity += GetBack();
    if (moveLeft) moveVelocity += GetLeft();
    if (moveRight) moveVelocity += GetRight();

    if (length(moveVelocity) > 0.0f)
    {
        m_bMoved = true;
        moveVelocity = normalize(moveVelocity) * moveSpeed;
    }

    moveVelocity = lerp(m_prevMoveVelocity, moveVelocity, 1.0f - exp(-delta_time * 20.0f));
    m_prevMoveVelocity = moveVelocity;

    m_pos += moveVelocity * delta_time;
    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
}

void Camera::SetupCameraCB(CameraConstant& cameraCB)
{
    Renderer* pRenderer = Engine::GetInstance()->GetRenderer();

    cameraCB.cameraPos = GetPosition();
    cameraCB.spreadAngle = atanf(2.0f * tanf(degree_to_radian(m_fov) * 0.5f) / pRenderer->GetRenderHeight()); // "Texture Level-of-Detail Strategies for Real-Time Ray Tracing", Eq.20
    cameraCB.nearZ = m_znear;
    cameraCB.linearZParams = CalcDepthLinearizationParams(m_projection);
    cameraCB.jitter = m_jitter;
    cameraCB.prevJitter = m_prevJitter;
    
    cameraCB.mtxView = m_view;
    cameraCB.mtxViewInverse = inverse(m_view);
    cameraCB.mtxProjection = m_projectionJitter;
    cameraCB.mtxProjectionInverse = inverse(m_projectionJitter);
    cameraCB.mtxViewProjection = m_viewProjectionJitter;
    cameraCB.mtxViewProjectionInverse = inverse(m_viewProjectionJitter);

    cameraCB.mtxViewProjectionNoJitter = m_viewProjection;
    cameraCB.mtxPrevViewProjectionNoJitter = m_prevViewProjection;
    cameraCB.mtxClipToPrevClipNoJitter = mul(m_prevViewProjection, inverse(m_viewProjection));
    cameraCB.mtxClipToPrevViewNoJitter = mul(m_prevView, inverse(m_viewProjection));

    cameraCB.mtxPrevView = m_prevView;
    cameraCB.mtxPrevViewProjection = m_prevViewProjectionJitter;
    cameraCB.mtxPrevViewProjectionInverse = inverse(m_prevViewProjectionJitter);

    cameraCB.culling.viewPos = m_frustumViewPos;

    cameraCB.physicalCamera.aperture = m_aperture;
    cameraCB.physicalCamera.shutterSpeed = 1.0f / m_shutterSpeed;
    cameraCB.physicalCamera.iso = m_iso;
    cameraCB.physicalCamera.exposureCompensation = m_exposureCompensation;

    for (int i = 0; i < 6; ++i)
    {
        cameraCB.culling.planes[i] = m_frustumPlanes[i];
    }
}

static int32_t GetJitterPhaseCount(int32_t renderWidth, int32_t displayWidth)
{
    const float basePhaseCount = 8.0f;
    const int32_t jitterPhaseCount = int32_t(basePhaseCount * pow((float(displayWidth) / renderWidth), 2.0f));
    return jitterPhaseCount;
}

static float Halton(int32_t index, int32_t base)
{
    float f = 1.0f, result = 0.0f;

    for (int32_t currentIndex = index; currentIndex > 0;) 
    {
        f /= (float)base;
        result = result + f * (float)(currentIndex % base);
        currentIndex = (uint32_t)(floorf((float)(currentIndex) / (float)(base)));
    }

    return result;
}

void Camera::UpdateJitter()
{
    if (m_bEnableJitter)
    {
        Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
        uint64_t frameIndex = pRenderer->GetFrameID();

        m_prevJitter = m_jitter;

        switch (pRenderer->GetTemporalUpscaleMode())
        {
        case TemporalSuperResolution::FSR2:
        case TemporalSuperResolution::DLSS:
        case TemporalSuperResolution::XeSS:
        {
            uint32_t phaseCount = GetJitterPhaseCount(pRenderer->GetRenderWidth(), pRenderer->GetDisplayWidth());
            uint32_t index = frameIndex % phaseCount;
            m_jitter.x = Halton((index % phaseCount) + 1, 2) - 0.5f;
            m_jitter.y = Halton((index % phaseCount) + 1, 3) - 0.5f;
            break;
        }
        default:
        {
            const float* Offset = nullptr;
            float Scale = 1.0f;
            // following work of Vaidyanathan et all: https://software.intel.com/content/www/us/en/develop/articles/coarse-pixel-shading-with-temporal-supersampling.html
            static const float Halton23_16[16][2] = { { 0.0f, 0.0f }, { 0.5f, 0.333333f }, { 0.25f, 0.666667f }, { 0.75f, 0.111111f }, { 0.125f, 0.444444f }, { 0.625f, 0.777778f }, { 0.375f ,0.222222f }, { 0.875f ,0.555556f }, { 0.0625f, 0.888889f }, { 0.562500f, 0.037037f }, { 0.3125f, 0.37037f }, { 0.8125f, 0.703704f }, { 0.1875f, 0.148148f }, { 0.6875f, 0.481481f }, { 0.4375f ,0.814815f }, { 0.9375f ,0.259259f } };
            static const float BlueNoise_16[16][2] = { { 1.5f, 0.59375f }, { 1.21875f, 1.375f }, { 1.6875f, 1.90625f }, { 0.375f, 0.84375f }, { 1.125f, 1.875f }, { 0.71875f, 1.65625f }, { 1.9375f ,0.71875f }, { 0.65625f ,0.125f }, { 0.90625f, 0.9375f }, { 1.65625f, 1.4375f }, { 0.5f, 1.28125f }, { 0.21875f, 0.0625f }, { 1.843750f, 0.312500f }, { 1.09375f, 0.5625f }, { 0.0625f, 1.21875f }, { 0.28125f, 1.65625f } };

            Offset = Halton23_16[frameIndex % 16];
            static float2 correctionOffset = { -55,0 };
            if (correctionOffset.x == -55)
            {
                correctionOffset = { 0,0 };
                for (int i = 0; i < 16; i++)
                {
                    correctionOffset += float2(Halton23_16[i][0], Halton23_16[i][1]);
                }
                correctionOffset /= 16.0f;
                correctionOffset = float2(0.5f, 0.5f) - correctionOffset;
            }

            m_jitter = { (Offset[0] + correctionOffset.x) * Scale - 0.5f, (Offset[1] + correctionOffset.y) * Scale - 0.5f };
            break;
        }
        }
    }
}

void Camera::UpdateMatrix()
{
    m_prevView = m_view;
    m_prevProjection = m_projection;
    m_prevViewProjection = m_viewProjection;
    m_prevViewProjectionJitter = m_viewProjectionJitter;

    m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
    m_view = inverse(m_world);
    m_viewProjection = mul(m_projection, m_view);

    if (m_bEnableJitter)
    {
        uint32_t width = Engine::GetInstance()->GetRenderer()->GetRenderWidth();
        uint32_t height = Engine::GetInstance()->GetRenderer()->GetRenderHeight();

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
    //m_frustumPlanes[4].x = matrix[0][2];
    //m_frustumPlanes[4].y = matrix[1][2];
    //m_frustumPlanes[4].z = matrix[2][2];
    //m_frustumPlanes[4].w = matrix[3][2];
    //m_frustumPlanes[4] = normalize_plane(m_frustumPlanes[4]);
    m_frustumPlanes[4] = float4(0.0f, 0.0f, 0.0f, 0.0f); // infinite far plane

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

    SetPerpective(m_aspectRatio, m_fov, m_znear);
}

void Camera::OnCameraSettingGui()
{
    ImGui::SliderFloat("Move Speed", &m_moveSpeed, 1.0f, 200.0f, "%.0f");

    bool perspective_updated = false;
    perspective_updated |= ImGui::SliderFloat("Fov", &m_fov, 5.0f, 135.0f, "%.0f");
    perspective_updated |= ImGui::SliderFloat("Near Plane", &m_znear, 0.0001f, 3.0f, "%.4f");

    if (perspective_updated)
    {
        SetPerpective(m_aspectRatio, m_fov, m_znear);
    }
}

void Camera::OnPostProcessSettingGui()
{
    if (ImGui::CollapsingHeader("Physical Camera"))
    {
        ImGui::SliderFloat("Aperture", &m_aperture, 0.7f, 32.0f, "f/%.1f");
        ImGui::SliderInt("Shutter Speed", &m_shutterSpeed, 1, 4000, "1/%d");
        ImGui::SliderInt("ISO", &m_iso, 100, 20000, "%d");
        ImGui::SliderFloat("Exposure Compensation", &m_exposureCompensation, -5.0f, 5.0f, "%.1f");
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
    psoDesc.cs = pRenderer->GetShader("view_frustum.hlsl", "main", GfxShaderType::CS);
    IGfxPipelineState* pso = pRenderer->GetPipelineState(psoDesc, "ViewFrustum PSO");

    pCommandList->SetPipelineState(pso);
    pCommandList->Dispatch(1, 1, 1);
}
