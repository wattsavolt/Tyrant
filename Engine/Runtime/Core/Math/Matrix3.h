
#pragma once

#include "Base/Base.h"
#include "Math/Vector3.h"

namespace tyr
{
	class Quaternion;

	/// Class representing a row major 3x3 matrix.
	class TYR_CORE_EXPORT Matrix3
	{
	private:
		struct EulerAngleOrderData
		{
			int a, b, c;
			float sign;
		};

	public:
		Matrix3() = default;

		constexpr Matrix3(const Matrix3&) = default;
		constexpr Matrix3& operator=(const Matrix3&) = default;

		constexpr Matrix3(ArgZero)
			:m{ {0.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 0.0f} }
		{ }

		constexpr Matrix3(ArgIdentity)
			:m{ {1.0f, 0.0f, 0.0f},
					{0.0f, 1.0f, 0.0f},
					{0.0f, 0.0f, 1.0f} }
		{ }
		
		constexpr Matrix3(float m00, float m01, float m02,
				float m10, float m11, float m12,
				float m20, float m21, float m22)
			:m{{m00, m01, m02}, {m10, m11, m12}, {m20, m21, m22}}
		{ }

		/// Construct a matrix from a quaternion. 
		explicit Matrix3(const Quaternion& rotation)
		{
			FromQuaternion(rotation);
		}

		/// Construct a matrix that performs rotation and scale. 
		explicit Matrix3(const Quaternion& rotation, const Vector3& scale)
		{
			FromQuaternion(rotation);

			for (int row = 0; row < 3; row++)
			{
				for (int col = 0; col < 3; col++)
					m[row][col] = scale[row]*m[row][col];
			}
		}

		/// Construct a matrix from an angle/axis pair. 
		explicit Matrix3(const Vector3& axis, const float& angle)
		{
			FromAxisAngle(axis, angle);
		}

		/// Construct a matrix from 3 orthonormal local axes. 
		explicit Matrix3(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
		{
			FromAxes(xAxis, yAxis, zAxis);
		}

		/// Construct a matrix from euler angles, YXZ ordering.
		/// @see  Matrix3::FromEulerAngles	 
		explicit Matrix3(const float& xAngle, const float& yAngle, const float& zAngle)
		{
			FromEulerAngles(xAngle, yAngle, zAngle);
		}

		/// Construct a matrix from euler angles, custom ordering.
		/// @see  Matrix3::FromEulerAngles
		explicit Matrix3(const float& xAngle, const float& yAngle, const float& zAngle, AxesOrder order)
		{
			FromEulerAngles(xAngle, yAngle, zAngle, order);
		}

		/// Returns a row of the matrix. 
		float* operator[] (uint row) const
		{
			TYR_ASSERT(row < 3);

			return (float*)m[row];
		}

		Vector3 GetRow(uint row) const;
		void SetRow(uint row, const Vector3& vec);
		Vector3 GetColumn(uint col) const;
		void SetColumn(uint col, const Vector3& vec);

		bool operator== (const Matrix3& rhs) const;
		bool operator!= (const Matrix3& rhs) const;

		Matrix3 operator+ (const Matrix3& rhs) const;
		Matrix3 operator- (const Matrix3& rhs) const;
		// Multiplies in left to right order
		Matrix3 operator* (const Matrix3& rhs) const;
		Matrix3 operator- () const;
		Matrix3 operator* (float rhs) const;

		friend Matrix3 operator* (float lhs, const Matrix3& rhs);

		/// Transforms the given vector by this matrix and returns the newly transformed vector. 
		Vector3 Multiply(const Vector3& vec) const;

		/// Returns a transpose of the matrix (switched columns and rows). 
		Matrix3 Transpose() const;

		/// Calculates an inverse of the matrix if it exists.
		///
		/// @param[out]	mat			Resulting matrix inverse.
		/// @param[in]	tolerance 	(optional) Tolerance to use when checking if determinant is zero (or near zero in this case).
		/// 						Zero determinant means inverse doesn't exist.
		/// @return					True if inverse exists, false otherwise.
		bool Inverse(Matrix3& mat, float tolerance = 1e-06f) const;

		/// Calculates an inverse of the matrix if it exists.
		///
		/// @param[in]	tolerance 	(optional) Tolerance to use when checking if determinant is zero (or near zero in this case).
		/// 						Zero determinant means inverse doesn't exist.
		///
		/// @return					Resulting matrix inverse if it exists, otherwise a zero matrix.	 
		Matrix3 Inverse(float tolerance = 1e-06f) const;

		/// Calculates the matrix determinant. 
		float Determinant() const;

		/// Decompose a Matrix3 to rotation and scale.
		/// @note	
		/// Matrix must consist only of rotation and uniform scale transformations, otherwise accurate results are not
		/// guaranteed. Applying non-uniform scale guarantees rotation portion will not be accurate. 
		void Decompose(Quaternion& rotation, Vector3& scale) const;

		void Orthonormalize();

		/// Converts an orthonormal matrix to axis angle representation.
		/// @note	Matrix must be orthonormal. 
		void ToAxisAngle(Vector3& axis, float& angle) const;

		/// Creates a rotation matrix from an axis angle representation. 
		void FromAxisAngle(const Vector3& axis, const float angle);

		/// Converts an orthonormal matrix to quaternion representation.
		/// @note  Matrix must be orthonormal. 
		void ToQuaternion(Quaternion& quat) const;

		/// Creates a rotation matrix from a quaternion representation. 
		void FromQuaternion(const Quaternion& quat);

		/// Creates a matrix from three axes. 
		void FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis);

		/// Converts an orthonormal matrix to a euler angle (pitch/yaw/roll) representation.
		///
		/// @param[in,out]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[in,out]	yAngle  Rotation about y axis. (AKA Yaw)
		/// @param[in,out]	zAngle 	Rotation about z axis. (AKA Roll)
		/// @return	True if unique solution was found, false otherwise.	 
		bool ToEulerAngles(float& xAngle, float& yAngle, float& zAngle) const;

		/// Creates a rotation matrix from the provided Pitch/Yaw/Roll angles.
		///
		/// @param[in]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[in]	yAngle	Rotation about y axis. (AKA Yaw)
		/// @param[in]	zAngle	Rotation about z axis. (AKA Roll)
		///
		/// @note	Matrix must be orthonormal.
		/// 		Since different values will be produced depending in which order are the rotations applied, this method assumes
		/// 		they are applied in YXZ order. If you need a specific order, use the overloaded "fromEulerAngles" method instead.
		 
		void FromEulerAngles(const float xAngle, const float yAngle, const float zAngle);

		/// Creates a rotation matrix from the provided Pitch/Yaw/Roll angles.
		///
		/// @param[in]	xAngle	Rotation about x axis. (AKA Pitch)
		/// @param[in]	yAngle	Rotation about y axis. (AKA Yaw)
		/// @param[in]	zAngle	Rotation about z axis. (AKA Roll)
		/// @param[in]	order 	The order in which rotations will be applied.
		///						Different rotations can be created depending on the order.
		///
		/// @note	Matrix must be orthonormal.	 
		void FromEulerAngles(const float xAngle, const float yAngle, const float zAngle, AxesOrder order);

		static constexpr const float c_Epsilon = 1e-06f;
		static const Matrix3 c_Zero;
		static const Matrix3 c_Identity;

	protected:
		friend class Matrix4;

		float m[3][3];
	};
}
