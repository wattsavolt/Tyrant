#include "Camera.h"

#include "Math/Quaternion.h"

namespace tyr
{
	Camera::Camera()
	{
		// Set default values.
		m_Position = Vector3::c_Zero;
		m_Up = Vector3::c_Up;
		m_Forward = Vector3::c_Forward;
		m_Fov = 90;
		m_Width = 1920;
		m_Height = 1080;
		m_NearZ = 1;
		m_FarZ = 2000;

		BuildProjection();
	}

	Camera::Camera(const Vector3& position, const Vector3& up, const Vector3& forward, float fov, float width, float height, float nearZ, float farZ)
		: m_Position(position), m_Up(up), m_Forward(forward), m_Fov(fov), m_Width(width), m_Height(height), m_NearZ(nearZ), m_FarZ(farZ)
	{
		BuildProjection();
	}

	Camera::~Camera()
	{

	}

	void Camera::SetYaw(float yaw)
	{
		const Quaternion rot(Vector3::c_Up, yaw);
		m_Forward = rot.Rotate(m_Forward);
		m_Up = rot.Rotate(m_Up);
	}

	void Camera::SetPitch(float pitch)
	{
		const Vector3 right = m_Forward.Cross(m_Up);
		const Quaternion rot(right, pitch);
		m_Forward = rot.Rotate(m_Forward);
		m_Up = rot.Rotate(m_Up);
	}

	void Camera::MoveForwardBack(float amount)
	{
		m_Position += m_Forward * amount;
	}

	void Camera::MoveRightLeft(float amount)
	{
		const Vector3 right = m_Forward.Cross(m_Up);
		m_Position += right * amount;
	}

	void Camera::MoveUpDown(float amount)
	{
		m_Position += m_Up * amount;
	}

	void Camera::Target(Vector3 target)
	{
		// Ensure position and target are not the same
		if (m_Position == target)
		{
			return;
		}

		Vector3 newForward = Vector3::SafeNormalize(target - m_Position);

		SetForward(newForward);
	}

	void Camera::SetForward(Vector3 forward)
	{
		float angle = m_Forward.AngleBetween(forward);
		float angleInDegrees = Math::c_RadToDeg * angle;

		Vector3 rot;
		if (angleInDegrees != 0.0f && angleInDegrees != 180.0f && angleInDegrees != 360.0f)
		{
			const Vector3 axis = Vector3::SafeNormalize(Vector3::Cross(m_Forward, forward));
			const Quaternion rot(axis, angle);
			m_Up = rot.Rotate(m_Up);
		}

		m_Forward = forward;
	}

	void Camera::SetUp(Vector3 up)
	{
		float angle = m_Up.AngleBetween(up);
		float angleInDegrees = Math::c_RadToDeg * angle;

		Vector3 rot;
		if (angleInDegrees != 0.0f && angleInDegrees != 180.0f && angleInDegrees != 360.0f)
		{
			const Vector3 axis = Vector3::SafeNormalize(Vector3::Cross(m_Up, up));
			const Quaternion rot(axis, angle);
			m_Forward = rot.Rotate(m_Forward);
		}

		m_Up = up;
	}

	void Camera::BuildProjection()
	{
		// Do reverse-z for greater floating-point precision
		m_Projection = Matrix4::CreatePerspective(m_Fov, m_Width / m_Height, m_FarZ, m_NearZ);
	}
}

