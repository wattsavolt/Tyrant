
#pragma once

#include "Math/Math.h"

#include "Math/Vector3.h"
#include "Math/Matrix3.h"
#include "Math/Vector4.h"

namespace tyr
{
	class Quaternion;

	/// Class representing a row-major 4x4 matrix. 
	class TYR_CORE_EXPORT Matrix4
	{
	public:
		Matrix4() = default;

		constexpr Matrix4(const Matrix4&) = default;
		constexpr Matrix4& operator=(const Matrix4&) = default;

		constexpr Matrix4(ArgZero)
			:m{ {0.0f, 0.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 0.0f, 0.0f} }
		{ }

		constexpr Matrix4(ArgIdentity)
			:m{ {1.0f, 0.0f, 0.0f, 0.0f},
					{0.0f, 1.0f, 0.0f, 0.0f},
					{0.0f, 0.0f, 1.0f, 0.0f},
					{0.0f, 0.0f, 0.0f, 1.0f} }
		{ }

		constexpr Matrix4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
			:m{		{m00, m01, m02, m03},
					{m10, m11, m12, m13},
					{m20, m21, m22, m23},
					{m30, m31, m32, m33} }
		{ }

		/// Creates a 4x4 transformation matrix with a zero translation part from a rotation/scaling 3x3 matrix. 
		constexpr explicit Matrix4(const Matrix3& mat3)
			:m{ {mat3.m[0][0], mat3.m[0][1], mat3.m[0][2], 0.0f},
				{mat3.m[1][0], mat3.m[1][1], mat3.m[1][2], 0.0f},
				{mat3.m[2][0], mat3.m[2][1], mat3.m[2][2], 0.0f},
				{0.0f, 0.0f, 0.0f, 1.0f} }
		{ }

		/// Returns a row of the matrix. 
		Vector4& operator[] (uint row)
		{
			TYR_ASSERT(row < 4);

			return *(Vector4*)m[row];
		}

		/// Returns a row of the matrix. 
		const Vector4& operator[] (uint row) const
		{
			TYR_ASSERT(row < 4);

			return *(Vector4*)m[row];
		}

		// Multiplies in left to right order
		Matrix4 operator* (const Matrix4 &rhs) const
		{
			Matrix4 prod;
			for (uint row = 0; row < 4; ++row)
			{
				for (uint col = 0; col < 4; ++col)
				{
					prod.m[row][col] = 
						m[row][0] * rhs.m[0][col] +
						m[row][1] * rhs.m[1][col] + 
						m[row][2] * rhs.m[2][col] +
						m[row][3] * rhs.m[3][col];
				}
			}
			return prod;
		}

		Matrix4 operator+ (const Matrix4 &rhs) const
		{
			Matrix4 r;

			r.m[0][0] = m[0][0] + rhs.m[0][0];
			r.m[0][1] = m[0][1] + rhs.m[0][1];
			r.m[0][2] = m[0][2] + rhs.m[0][2];
			r.m[0][3] = m[0][3] + rhs.m[0][3];

			r.m[1][0] = m[1][0] + rhs.m[1][0];
			r.m[1][1] = m[1][1] + rhs.m[1][1];
			r.m[1][2] = m[1][2] + rhs.m[1][2];
			r.m[1][3] = m[1][3] + rhs.m[1][3];

			r.m[2][0] = m[2][0] + rhs.m[2][0];
			r.m[2][1] = m[2][1] + rhs.m[2][1];
			r.m[2][2] = m[2][2] + rhs.m[2][2];
			r.m[2][3] = m[2][3] + rhs.m[2][3];

			r.m[3][0] = m[3][0] + rhs.m[3][0];
			r.m[3][1] = m[3][1] + rhs.m[3][1];
			r.m[3][2] = m[3][2] + rhs.m[3][2];
			r.m[3][3] = m[3][3] + rhs.m[3][3];

			return r;
		}

		Matrix4 operator- (const Matrix4 &rhs) const
		{
			Matrix4 r;
			r.m[0][0] = m[0][0] - rhs.m[0][0];
			r.m[0][1] = m[0][1] - rhs.m[0][1];
			r.m[0][2] = m[0][2] - rhs.m[0][2];
			r.m[0][3] = m[0][3] - rhs.m[0][3];

			r.m[1][0] = m[1][0] - rhs.m[1][0];
			r.m[1][1] = m[1][1] - rhs.m[1][1];
			r.m[1][2] = m[1][2] - rhs.m[1][2];
			r.m[1][3] = m[1][3] - rhs.m[1][3];

			r.m[2][0] = m[2][0] - rhs.m[2][0];
			r.m[2][1] = m[2][1] - rhs.m[2][1];
			r.m[2][2] = m[2][2] - rhs.m[2][2];
			r.m[2][3] = m[2][3] - rhs.m[2][3];

			r.m[3][0] = m[3][0] - rhs.m[3][0];
			r.m[3][1] = m[3][1] - rhs.m[3][1];
			r.m[3][2] = m[3][2] - rhs.m[3][2];
			r.m[3][3] = m[3][3] - rhs.m[3][3];

			return r;
		}

		inline bool operator== (const Matrix4& rhs) const
		{
			if (m[0][0] != rhs.m[0][0] || m[0][1] != rhs.m[0][1] || m[0][2] != rhs.m[0][2] || m[0][3] != rhs.m[0][3] ||
				m[1][0] != rhs.m[1][0] || m[1][1] != rhs.m[1][1] || m[1][2] != rhs.m[1][2] || m[1][3] != rhs.m[1][3] ||
				m[2][0] != rhs.m[2][0] || m[2][1] != rhs.m[2][1] || m[2][2] != rhs.m[2][2] || m[2][3] != rhs.m[2][3] ||
				m[3][0] != rhs.m[3][0] || m[3][1] != rhs.m[3][1] || m[3][2] != rhs.m[3][2] || m[3][3] != rhs.m[3][3])
			{
				return false;
			}

			return true;
		}

		inline bool operator!= (const Matrix4& rhs) const
		{
			return !operator==(rhs);
		}

		Matrix4 operator*(float rhs) const
		{
			return Matrix4(rhs*m[0][0], rhs*m[0][1], rhs*m[0][2], rhs*m[0][3],
				rhs*m[1][0], rhs*m[1][1], rhs*m[1][2], rhs*m[1][3],
				rhs*m[2][0], rhs*m[2][1], rhs*m[2][2], rhs*m[2][3],
				rhs*m[3][0], rhs*m[3][1], rhs*m[3][2], rhs*m[3][3]);
		}

		/// Returns the specified column of the matrix, ignoring the last row. 
		Vector3 GetColumn(uint col) const
		{
			TYR_ASSERT(col < 4);

			return Vector3(m[0][col], m[1][col], m[2][col]);
		}

		/// Returns the specified column of the matrix. 
		Vector4 GetColumn4D(uint col) const
		{
			TYR_ASSERT(col < 4);

			return Vector4(m[0][col], m[1][col], m[2][col], m[3][col]);
		}

		/// Returns a transpose of the matrix ie. switches rows and columns. 
		Matrix4 Transpose() const
		{
			return Matrix4(m[0][0], m[1][0], m[2][0], m[3][0],
				m[0][1], m[1][1], m[2][1], m[3][1],
				m[0][2], m[1][2], m[2][2], m[3][2],
				m[0][3], m[1][3], m[2][3], m[3][3]);
		}

		void MakeProjectionRightHanded() 
		{
			m[2][3] = -m[2][3];
			m[3][2] = -m[3][2];
		}

		// Creates from a left handed projection matrix and flips the z
		inline Matrix4 CreateRightHandedViewProjection() const
		{
			return Matrix4(m[0][0],  m[0][1], m[0][2], m[0][3],
				 m[1][0], m[1][1], m[1][2], m[1][3],
				 -m[2][0], -m[2][1], -m[2][2], -m[2][3],
				 m[3][0], m[3][1], -m[3][2], m[3][3]);
		}

		// Creates from a left handed projection matrix and flips the z
		inline Matrix4 CreateRightHandedProjection() const
		{
			return Matrix4(m[0][0], m[0][1], m[0][2], m[0][3],
				m[1][0], m[1][1], m[1][2], m[1][3],
				m[2][0], m[2][1], m[2][2], -m[2][3],
				m[3][0], m[3][1], -m[3][2], m[3][3]);
		}

		/// Assigns the vector to a column of the matrix. 
		void SetColumn(uint idx, const Vector4& column)
		{
			m[0][idx] = column.x;
			m[1][idx] = column.y;
			m[2][idx] = column.z;
			m[3][idx] = column.w;
		}

		/// Assigns the vector to a row of the matrix. 
		void SetRow(uint idx, const Vector4& column)
		{
			m[idx][0] = column.x;
			m[idx][1] = column.y;
			m[idx][2] = column.z;
			m[idx][3] = column.w;
		}

		/// Returns the rotation/scaling part of the matrix as a 3x3 matrix. 
		Matrix3 GetMatrix3() const
		{
			Matrix3 mat3;
			mat3.m[0][0] = m[0][0];
			mat3.m[0][1] = m[0][1];
			mat3.m[0][2] = m[0][2];
			mat3.m[1][0] = m[1][0];
			mat3.m[1][1] = m[1][1];
			mat3.m[1][2] = m[1][2];
			mat3.m[2][0] = m[2][0];
			mat3.m[2][1] = m[2][1];
			mat3.m[2][2] = m[2][2];

			return mat3;
		}

		/// Calculates the adjoint of the matrix. 
		Matrix4 Adjoint() const;

		/// Calculates the determinant of the matrix. 
		float Determinant() const;

		/// Calculates the determinant of the 3x3 sub-matrix. 
		float Determinant3x3() const;

		/// Calculates the inverse of the matrix. 
		Matrix4 Inverse() const;

		/// Creates a matrix from translation, rotation and scale.
		/// @note	The transformation is applied in scale->rotation->translation order. 
		void SetTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

		/// Creates a matrix from inverse translation, rotation and scale.
		/// @note	This is cheaper than setTRS() and then performing inverse().
		void SetInverseTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

		/// Extracts the translation from the matrix. 
		Vector3 GetTranslation() const { return Vector3(m[0][3], m[1][3], m[2][3]); }

		/// Check whether or not the matrix is affine matrix.
		/// @note	An affine matrix is a 4x4 matrix with row 3 equal to (0, 0, 0, 1), meaning no projective coefficients. 
		bool IsAffine() const
		{
			return m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0 && m[3][3] == 1;
		}

		/// Returns the inverse of the affine matrix.
		/// @note	Matrix must be affine.
		Matrix4 InverseAffine() const;

		/// Concatenate two affine matrices.
		/// @note	Both matrices must be affine.
		Matrix4 ConcatenateAffine(const Matrix4 &other) const
		{
			return Matrix4
			(
				m[0][0] * other.m[0][0] + m[0][1] * other.m[1][0] + m[0][2] * other.m[2][0],
				m[0][0] * other.m[0][1] + m[0][1] * other.m[1][1] + m[0][2] * other.m[2][1],
				m[0][0] * other.m[0][2] + m[0][1] * other.m[1][2] + m[0][2] * other.m[2][2],
				m[0][0] * other.m[0][3] + m[0][1] * other.m[1][3] + m[0][2] * other.m[2][3] + m[0][3],

				m[1][0] * other.m[0][0] + m[1][1] * other.m[1][0] + m[1][2] * other.m[2][0],
				m[1][0] * other.m[0][1] + m[1][1] * other.m[1][1] + m[1][2] * other.m[2][1],
				m[1][0] * other.m[0][2] + m[1][1] * other.m[1][2] + m[1][2] * other.m[2][2],
				m[1][0] * other.m[0][3] + m[1][1] * other.m[1][3] + m[1][2] * other.m[2][3] + m[1][3],

				m[2][0] * other.m[0][0] + m[2][1] * other.m[1][0] + m[2][2] * other.m[2][0],
				m[2][0] * other.m[0][1] + m[2][1] * other.m[1][1] + m[2][2] * other.m[2][1],
				m[2][0] * other.m[0][2] + m[2][1] * other.m[1][2] + m[2][2] * other.m[2][2],
				m[2][0] * other.m[0][3] + m[2][1] * other.m[1][3] + m[2][2] * other.m[2][3] + m[2][3],

				0, 0, 0, 1
			);
		}

		/// Transform a 3D point by this matrix.
	    /// Matrix must be affine, if it is not use Multiply() function.
		/// Affine matrices are translation, scale, shear and rotation
		Vector3 MultiplyAffine(const Vector3& v) const
		{
			Vector3 output;
			for (uint i = 0; i < 3; ++i)
			{
				output[i] =
					v[0] * m[0][i] +
					v[1] * m[1][i] +
					v[2] * m[2][i];
			}

			return output;
		}

		/// Transform a 4D vector by this matrix.
		/// Matrix must be affine, if it is not use Multiply() function. 
		/// Affine matrices are scale, rotation, translation and shear
		Vector4 MultiplyAffine(const Vector4& v) const
		{
			return Vector4
			(
				m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w,
				m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
				m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
				v.w
			);
		}

		/// Transform a 3D direction by this matrix. 
		Vector3 MultiplyDirection(const Vector3& v) const
		{
			return Vector3
			(
				m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z,
				m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z,
				m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z
			);
		}

		/// Transform a 4D vector by this matrix.
		/// @note If the matrix doesn't contain a projection, it's faster to use MultiplyAffine() 	
		Vector4 Multiply(const Vector4& v) const
		{
			return Vector4
			(
				m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w,
				m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
				m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
				m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w
			);
		}

		/// Creates a view matrix from a position, forward vector and up vector. 
		void LookAt(const Vector3& position, const Vector3& forward, const Vector3& up);

		/// Creates a view matrix from a position and rotation. 
		void LookAt(const Vector3& position, const Quaternion& rotation);

		/// Creates an ortographic projection matrix that scales the part of the view bounded by @p left, @p right,
		/// @p top and @p bottom into [-1, 1] range. If @p far is non-zero the matrix will also transform the depth into
		/// [-1, 1] range, otherwise it will leave it as-is.
		void ToOrthographic(float left, float right, float top, float bottom, float near, float far);

		/// Creates a 4x4 transformation matrix that performs translation. 
		static Matrix4 CreateTranslation(const Vector3& translation);

		/// Creates a 4x4 transformation matrix that performs scaling. 
		static Matrix4 CreateScaling(const Vector3& scale);

		/// Creates a 4x4 transformation matrix that performs uniform scaling. 
		static Matrix4 CreateScaling(float scale);

		/// Creates a 4x4 transformation matrix that performs rotation. 
		static Matrix4 CreateRotation(const Quaternion& rotation);

		/// Creates a 4x4 left-handed perspective projection matrix.
		///
		/// @note Do reverse-z to allow greater floating-point precision by switching near with far
		/// @param[in]	fov		    Horizontal field of view in degrees.
		/// @param[in]	aspect		Aspect ratio. Determines the vertical field of view.
		/// @param[in]	near		Distance to the near plane.
		/// @param[in]	far			Distance to the far plane.
		static Matrix4 CreatePerspective(float fov, float aspect, float near, float far);

		static Matrix4 CreateOrthographic(float left, float right, float top, float bottom, float near, float far);

		/// Creates a view matrix from a position, forward vector and up vector. 
		static Matrix4 CreateView(const Vector3& position, const Vector3& forward, const Vector3& up);

		/// Creates a view matrix from a positon and rotation. 
		static Matrix4 CreateView(const Vector3& position, const Quaternion& rotation);

		/// Creates a matrix from translation, rotation and scale.
		/// @note	The transformation are applied in scale->rotation->translation order. 
		static Matrix4 CreateTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

		/// Creates a matrix from inverse translation, rotation and scale.
		/// @note	This is cheaper than setTRS() and then performing inverse().	 
		static Matrix4 CreateInverseTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale);

		static const Matrix4 c_Zero;
		static const Matrix4 c_Identity;

	private:
		float m[4][4];
	};
}
