#include "camera.h"
#include "engine.h"

Camera::Camera() : m_resizeConnection({})
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

void Camera::UpdateMatrix()
{
	m_world = mul(translation_matrix(m_pos), rotation_matrix(rotation_quat(m_rotation)));
	m_view = inverse(m_world);
	m_viewProjection = mul(m_projection, m_view);
}

void Camera::OnWindowResize(uint32_t width, uint32_t height)
{
	SetPerpective((float)width / height, m_fov, m_znear, m_zfar);
}
