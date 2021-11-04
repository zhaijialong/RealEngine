#include "camera.h"
#include "core/engine.h"
#include "imgui/imgui.h"

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
	UpdateMatrix();
}

void Camera::SetPosition(const float3& pos)
{
	if (m_pos != pos)
	{
		m_pos = pos;
		UpdateMatrix();
	}
}

void Camera::SetRotation(const float3& rotation)
{
	if (m_rotation != rotation)
	{
		m_rotation = rotation;
		UpdateMatrix();
	}
}

void Camera::SetFov(float fov)
{
	m_fov = fov;

	SetPerpective(m_aspectRatio, m_fov, m_znear, m_zfar);
}

void Camera::Tick(float delta_time)
{
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
	m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
	m_view = inverse(m_world);
	m_viewProjection = mul(m_projection, m_view);

	if (m_bEnableJitter)
	{
		uint32_t width = Engine::GetInstance()->GetRenderer()->GetBackbufferWidth();
		uint32_t height = Engine::GetInstance()->GetRenderer()->GetBackbufferHeight();

		float4x4 mtxJitter = translation_matrix(float3(2.0f * m_jitter.x / (float)width, -2.0f * m_jitter.y / (float)height, 0.0f));
		m_viewProjection = mul(mtxJitter, m_viewProjection);
	}
}

void Camera::OnWindowResize(void* window, uint32_t width, uint32_t height)
{
	m_aspectRatio = (float)width / height;

	SetPerpective(m_aspectRatio, m_fov, m_znear, m_zfar);
}
