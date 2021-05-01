#include "camera.h"

Camera::Camera()
{
	m_pos = float3(0.0f, 0.0f, 0.0f);
	m_rotation = float3(0.0, 0.0, 0.0f);
}

void Camera::SetPerpective(float aspectRatio, float yfov, float znear, float zfar)
{
	m_projection = perspective_matrix(yfov, aspectRatio, znear, zfar, pos_z, zero_to_one);
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
