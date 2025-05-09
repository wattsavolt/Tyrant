
#pragma once

#include "Math/Math.h"
#include "Math/Vector3.h"


namespace tyr
{
	class Matrix3;

	/// Represents a quaternion used for 3D rotations. 
	class TYR_CORE_EXPORT Quaternion
	{
	private:
		struct EulerAngleOrderData
		{
			int a, b, c;
		};

	public:
		Quaternion() = default;

		constexpr Quaternion(const Quaternion&) = default;
		constexpr Quaternion& operator=(const Quaternion&) = default;

		constexpr Quaternion(ArgZero)
			: x(0), y(0), z(0), w(0)
		{ }

		constexpr Quaternion(ArgIdentity)
			: x(0), y(0), z(0), w(1)
		{ }

		constexpr Quaternion(float w, float x, float y, float z)
			: x(x), y(y), z(z), w(w)
		{ }

		/// Construct from a rotation matrix. 
		explicit Quaternion(const Matrix3& rotMat)
		{
			FromRotationMatrix(rotMat);
		}

		/// Construct from an angle/axis. 
		explicit Quaternion(const Vector3& axis, float angle)
		{
			FromAxisAngle(axis, angle);
		}

		/// Construct from 3 orthonormal local axes. 
		explicit Quaternion(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
		{
			FromAxes(xAxis, yAxis, zAxis);
		}

		/// Construct a quaternion from euler angles, YXZ ordering.
		/// @see		Quaternion::fromEulerAngles 
		explicit Quaternion(float xAngle, float yAngle, float zAngle)
		{
			FromEulerAngles(xAngle, yAngle, zAngle);
		}

		/// Construct a quaternion from euler angles, custom ordering.
		/// @see		Quaternion::fromEulerAngles	 
		explicit Quaternion(float xAngle, float yAngle, float zAngle, AxesOrder order)
		{
			FromEulerAngles(xAngle, yAngle, zAngle, order);
		}

		/// Exchange the contents of this quaternion with another. 
		void Swap(Quaternion& q)
		{
			std::swap(w, q.w);
			std::swap(x, q.x);
			std::swap(y, q.y);
			std::swap(z, q.z);
		}

		float operator[] (const size_t i) const
		{
			TYR_ASSERT(i < 4);

			return *(&x+i);
		}

		float& operator[] (const size_t i)
		{
			TYR_ASSERT(i < 4);

			return *(&x+i);
		}

		/// Initializes the quaternion from a 3x3 rotation matrix.
		/// @note	It's up to the caller to ensure the matrix is orthonormal. 
		void FromRotationMatrix(const Matrix3& mat);

		/// Initializes the quaternion from an angle axis pair. Quaternion will represent a rotation of "angle" radians
		/// around "axis".
		void FromAxisAngle(const Vector3& axis, float angle);

		/// Initializes the quaternion from orthonormal set of axes. Quaternion will represent a rotation from base axes
	    /// to the specified set of axes.
		///
		/// @note	The axes must be orthonormal. 
		void FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis);

		/// Creates a quaternion from the provided Pitch/Yaw/Roll angles.
		///
		/// @param[in]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[in]	yAngle	Rotation about y axis. (AKA Yaw)
		/// @param[in]	zAngle	Rotation about z axis. (AKA Roll)
		///
		/// @note
		/// Since different values will be produced depending on the order of the rotations, this method assumes
		/// they are in YXZ order. For another order, use the overloaded FromEulerAngles() function instead.	 
		void FromEulerAngles(float xAngle, float yAngle, float zAngle);

		/// Creates a quaternion from the provided Pitch/Yaw/Roll angles.
		///
		/// @param[in]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[in]	yAngle	Rotation about y axis. (AKA Yaw)
		/// @param[in]	zAngle	Rotation about z axis. (AKA Roll)
		/// @param[in]	order 	The order in which rotations will be extracted. Different values can be retrieved depending
		///				on the order. 
		void FromEulerAngles(float xAngle, float yAngle, float zAngle, AxesOrder order);

		/// Converts a quaternion to a rotation matrix.
		void ToRotationMatrix(Matrix3& mat) const;

		/// Converts a quaternion to an angle axis pair.
		///
		/// @param[out]	axis 	The axis around the which rotation takes place.
		/// @param[out]	angle	The angle in radians determining amount of rotation around the axis.
		void ToAxisAngle(Vector3& axis, float& angle) const;

		/// Converts a quaternion to an orthonormal set of axes.
		///
		/// @param[out]	xAxis	The X axis.
		/// @param[out]	yAxis	The Y axis.
		/// @param[out]	zAxis	The Z axis.
		void ToAxes(Vector3& xAxis, Vector3& yAxis, Vector3& zAxis) const;

		/// Extracts Pitch/Yaw/Roll rotations from this quaternion. Angles are in radians.
		///
		/// @param[out]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[out]	yAngle  Rotation about y axis. (AKA Yaw)
		/// @param[out]	zAngle 	Rotation about z axis. (AKA Roll)
		///
		/// @return	True if unique solution was found, false otherwise. 
		bool ToEulerAngles(float& xAngle, float& yAngle, float& zAngle) const;

		/// Gets the positive x-axis of the coordinate system transformed by this quaternion. 
		Vector3 Right() const;

		/// Gets the positive y-axis of the coordinate system transformed by this quaternion. 
		Vector3 Up() const;

		/// Gets the positive z-axis of the coordinate system transformed by this quaternion. 
		Vector3 Forward() const;


		Quaternion operator+ (const Quaternion& rhs) const
		{
			return Quaternion(w + rhs.w, x + rhs.x, y + rhs.y, z + rhs.z);
		}

		Quaternion operator- (const Quaternion& rhs) const
		{
			return Quaternion(w - rhs.w, x - rhs.x, y - rhs.y, z - rhs.z);
		}

		Quaternion operator* (const Quaternion& rhs) const
		{
			return Quaternion
			(
				w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
				w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
				w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z,
				w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x
			);
		}

		Quaternion operator* (float rhs) const
		{
			return Quaternion(rhs * w, rhs * x, rhs * y, rhs * z);
		}

		Quaternion operator/ (float rhs) const
		{
			TYR_ASSERT(rhs != 0.0);

			const float inv = 1.0f / rhs;
			return Quaternion(w * inv, x * inv, y * inv, z * inv);
		}

		Quaternion operator- () const
		{
			return Quaternion(-w, -x, -y, -z);
		}

		bool operator== (const Quaternion& rhs) const
		{
			return (rhs.x == x) && (rhs.y == y) && (rhs.z == z) && (rhs.w == w);
		}

		bool operator!= (const Quaternion& rhs) const
		{
			return !operator==(rhs);
		}

		Quaternion& operator+= (const Quaternion& rhs)
		{
			w += rhs.w;
			x += rhs.x;
			y += rhs.y;
			z += rhs.z;

			return *this;
		}

		Quaternion& operator-= (const Quaternion& rhs)
		{
			w -= rhs.w;
			x -= rhs.x;
			y -= rhs.y;
			z -= rhs.z;

			return *this;
		}

		Quaternion& operator*= (const Quaternion& rhs)
		{
			float newW = w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z;
			float newX = w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y;
			float newY = w * rhs.y + y * rhs.w + z * rhs.x - x * rhs.z;
			float newZ = w * rhs.z + z * rhs.w + x * rhs.y - y * rhs.x;

			w = newW;
			x = newX;
			y = newY;
			z = newZ;

			return *this;
		}

		friend Quaternion operator* (float lhs, const Quaternion& rhs)
		{
			return Quaternion(lhs * rhs.w, lhs * rhs.x, lhs * rhs.y, lhs * rhs.z);
		}
		
		/// Calculates the dot product of this quaternion and another. 
		float Dot(const Quaternion& other) const
		{
			return w * other.w + x * other.x + y * other.y + z * other.z;
		}

		/// Normalizes this quaternion and returns the previous length. 
		float Normalize()
		{
			float len = Math::Sqrt(Dot(*this, *this));
			*this = *this * (1.0f / len);	
			return len;
		}

		/// Normalizes this quaternion avoiding division by 0 and returns the previous length. 
		float SafeNormalize(float tolerance = 1e-04f)
		{
			float len = Math::Sqrt(Dot(*this, *this));
			if (len > (tolerance * tolerance))
			{
				*this = *this * (1.0f / len);
			}
			return len;
		}

		/// Gets the conjugate. 
		Quaternion Conjugate() const
		{
			return Quaternion(w, -x, -y, -z);
		}

		/// Gets the inverse.
		/// @note	Quaternion must be non-zero. 
		Quaternion Inverse() const;

		/// Rotates the provided vector. 
		Vector3 Rotate(const Vector3& v) const
		{
			const Vector3 u(x, y, z);
			Vector3 vOut = (2.0f * (u.Dot(v))) * u; 
			vOut += ((w * w) - (u.Dot(u))) * v;
			vOut += 2.0f * w * (u.Cross(v));

			return vOut;
		}

		/// Orients the quaternion so its negative z axis points to the provided direction.
		///
		/// @param[in]	forwardDir	Direction to orient towards.
		void LookRotation(const Vector3& forward);

		/// Orients the quaternion so its negative z axis points to the provided direction.
		///
		/// @param[in]	forward	Direction to orient towards.
		/// @param[in]	up		Constrains y axis orientation to a plane this vector lies on. This rule might be broken
		///						if forward and up direction are nearly parallel. 
		void LookRotation(const Vector3& forward, const Vector3& up);

		/// Query if any of the components of the quaternion are not a number. 
		bool IsNaN() const
		{
			return Math::IsNaN(x) || Math::IsNaN(y) || Math::IsNaN(z) || Math::IsNaN(w);
		}

		/// Calculates the dot product between two quaternions. 
		static float Dot(const Quaternion& lhs, const Quaternion& rhs)
		{
			return lhs.w * rhs.w + lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
		}

		/// Normalizes the provided quaternion. 	 
		static Quaternion Normalize(const Quaternion& q)
		{
			float sqrdLen = Dot(q, q);
			return q * Math::InvSqrt(sqrdLen);
		}

		/// Normalizes the provided quaternion and prevents division by zero. 
		static Quaternion SafeNormalize(const Quaternion& q, float tolerance = 1e-04f)
		{
			float sqrdLen = Dot(q, q);
			if (sqrdLen > tolerance)
			{
				return q * Math::InvSqrt(sqrdLen);
			}
			return q;
		}

		/// Performs spherical interpolation between two quaternions. Spherical interpolation neatly interpolates between
		/// two rotations without modifying the size of the vector it is applied to (unlike linear interpolation). 
		static Quaternion Slerp(float t, const Quaternion& p, const Quaternion& q, bool shortestPath = true);

		/// Linearly interpolates between the two quaternions using @p t. t should be in [0, 1] range, where t = 0
		/// corresponds to the left vector, while t = 1 corresponds to the right vector.	
		static Quaternion Lerp(float t, const Quaternion& a, const Quaternion& b)
		{
			float d = Dot(a, b);
			float flip = d >= 0.0f ? 1.0f : -1.0f;

			Quaternion output = flip * (1.0f - t) * a + t * b;
			return SafeNormalize(output);
		}

		/// Gets the shortest arc quaternion to rotate this vector to the destination vector. 
		static Quaternion GetRotationBetweenVectors(const Vector3& from, const Vector3& dest, const Vector3& fallbackAxis = Vector3::c_Zero);

		static constexpr float c_Epsilon = 1e-03f;

		static const Quaternion c_Zero;
		static const Quaternion c_Identity;

		float x, y, z, w; 
	};
}

