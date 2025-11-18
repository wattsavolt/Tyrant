#pragma once

#include "Core.h"
#include "Math/Vector3.h"
#include "Math/Matrix4.h"
#include "EngineMacros.h"

namespace tyr
{
	/// Allows for movement of the camera in the scene and calculates view projection matrix.
	class TYR_ENGINE_EXPORT Camera final
	{
	public:
		Camera();
		Camera(const Vector3& position, const Vector3& up, const Vector3& forward, float fov, float nearZ, float farZ);
		~Camera();
		void SetPitch(float p);
		void SetYaw(float y);
		void MoveForwardBack(float amount);
		void MoveRightLeft(float amount);
		void MoveUpDown(float amount);
		void Target(Vector3 newTarget);
		void SetForward(Vector3 forward);
		void SetUp(Vector3 newUp);

		void SetPosition(const Vector3& position)
		{
			m_Position = position;
		}

		const Vector3& GetPosition() const
		{
			return m_Position;
		}

		const Vector3& GetUp() const
		{
			return m_Up;
		}

		const Vector3& GetForward() const
		{
			return m_Forward;
		}

		void SetFOV(float fov)
		{
			m_Fov = fov;
		}

		float GetFOV() const
		{
			return m_Fov;
		}

		void SetNearZ(float nearZ)
		{
			m_NearZ = nearZ;
		}

		float GetNearZ() const
		{
			return m_NearZ;
		}

		void SetFarZ(float farZ)
		{
			m_FarZ = farZ;
		}

		float GetFarZ() const
		{
			return m_FarZ;
		}

	private:
		Vector3 m_Position;
		Vector3 m_Forward;
		Vector3 m_Up;
		// Field of view in degrees
		float m_Fov;
		float m_NearZ;
		float m_FarZ;
	};
}