#pragma once

#include "utils/math.h"
#include "lsignal/lsignal.h"

class Camera
{
public:
	Camera();
	~Camera();

	void SetPerpective(float aspectRatio, float yfov, float znear, float zfar);

	void SetPosition(const float3& pos);
	const float3& GetPosition() const { return m_pos; }

	void SetRotation(const float3& rotation);
	const float3& GetRotation() const { return m_rotation; }

	const float4x4& GetViewMatrix() const { return m_view; }
	const float4x4& GetProjectionMatrix() const { return m_projection; }
	const float4x4& GetViewProjectionMatrix() const { return m_viewProjection; }

	float3 GetLeft() const { return -m_world[0].xyz(); }
	float3 GetRight() const { return m_world[0].xyz(); }
	float3 GetForward() const { return m_world[2].xyz(); }
	float3 GetBack() const { return -m_world[2].xyz(); }
	float3 GetUp() const { return m_world[1].xyz(); }
	float3 GetDown() const { return -m_world[1].xyz(); }

	float GetMoveSpeed() const { return m_moveSpeed; }
	void SetMoveSpeed(float speed) { m_moveSpeed = speed; }

	void Tick(float delta_time);

private:
	void UpdateMatrix();
	void OnWindowResize(void* window, uint32_t width, uint32_t height);

private:
	float3 m_pos;
	float3 m_rotation; //in degrees

	float4x4 m_world;
	float4x4 m_view;
	float4x4 m_projection;
	float4x4 m_viewProjection;

	float m_fov = 0.0f;
	float m_znear = 0.0f;
	float m_zfar = 0.0f;
	float m_moveSpeed = 10.0f;

	lsignal::connection m_resizeConnection;
};