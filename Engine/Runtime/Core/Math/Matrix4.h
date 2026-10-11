
#pragma once

#include "Math/Math.h"

#include "Math/Vector3.h"
#include "Math/Matrix3.h"
#include "Math/Vector4.h"
#include "Math/SIMD.h"

namespace tyr
{
	class Quaternion;

	/// Class representing a row-major 4x4 matrix. 
	class TYR_CORE_API Matrix4
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

		// Multiplies in left to right order. Two result rows at a time, each a sum of rhs's rows
		// weighted by that row's elements.
		Matrix4 operator* (const Matrix4 &rhs) const
		{
			const Reg rhsRow0 = SIMD::Broadcast4(rhs.m[0]);
			const Reg rhsRow1 = SIMD::Broadcast4(rhs.m[1]);
			const Reg rhsRow2 = SIMD::Broadcast4(rhs.m[2]);
			const Reg rhsRow3 = SIMD::Broadcast4(rhs.m[3]);

			Matrix4 prod;
			for (uint row = 0; row < 4; row += 2)
			{
				const Reg rows = SIMD::Load(m[row]);
				Reg result = SIMD::Mul(SIMD::SplatHalves<0>(rows), rhsRow0);
				result = SIMD::Add(result, SIMD::Mul(SIMD::SplatHalves<1>(rows), rhsRow1));
				result = SIMD::Add(result, SIMD::Mul(SIMD::SplatHalves<2>(rows), rhsRow2));
				result = SIMD::Add(result, SIMD::Mul(SIMD::SplatHalves<3>(rows), rhsRow3));
				SIMD::Store(prod.m[row], result);
			}
			return prod;
		}

		Matrix4 operator+ (const Matrix4 &rhs) const
		{
			Matrix4 r;
			SIMD::Store(r.m[0], SIMD::Add(SIMD::Load(m[0]), SIMD::Load(rhs.m[0])));
			SIMD::Store(r.m[2], SIMD::Add(SIMD::Load(m[2]), SIMD::Load(rhs.m[2])));
			return r;
		}

		Matrix4 operator- (const Matrix4 &rhs) const
		{
			Matrix4 r;
			SIMD::Store(r.m[0], SIMD::Sub(SIMD::Load(m[0]), SIMD::Load(rhs.m[0])));
			SIMD::Store(r.m[2], SIMD::Sub(SIMD::Load(m[2]), SIMD::Load(rhs.m[2])));
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
			const Reg scale = SIMD::Set(rhs);
			Matrix4 r;
			SIMD::Store(r.m[0], SIMD::Mul(SIMD::Load(m[0]), scale));
			SIMD::Store(r.m[2], SIMD::Mul(SIMD::Load(m[2]), scale));
			return r;
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
			Reg128 row0 = SIMD::Load4(m[0]);
			Reg128 row1 = SIMD::Load4(m[1]);
			Reg128 row2 = SIMD::Load4(m[2]);
			Reg128 row3 = SIMD::Load4(m[3]);
			SIMD::Transpose4(row0, row1, row2, row3);

			Matrix4 r;
			SIMD::Store4(r.m[0], row0);
			SIMD::Store4(r.m[1], row1);
			SIMD::Store4(r.m[2], row2);
			SIMD::Store4(r.m[3], row3);
			return r;
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

		/// The reverse of SetTRS. Only exact for matrices without shear.
		void DecomposeTRS(Vector3& translation, Quaternion& rotation, Vector3& scale) const;

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

		/// Transform a 3D point by this matrix, including its translation row.
	    /// Matrix must be affine, if it is not use Multiply() function.
		/// Affine matrices are translation, scale, shear and rotation
		Vector3 MultiplyAffine(const Vector3& v) const
		{
			const Reg128 result = SIMD::Add4(MultiplyRows3(v.x, v.y, v.z), SIMD::Load4(m[3]));
			return Vector3(StoreLane<0>(result), StoreLane<1>(result), StoreLane<2>(result));
		}

		/// Transform a 4D vector by this matrix.
		/// Matrix must be affine, if it is not use Multiply() function.
		/// Affine matrices are scale, rotation, translation and shear
		Vector4 MultiplyAffine(const Vector4& v) const
		{
			Vector4 output = Multiply(v);
			output.w = v.w;
			return output;
		}

		/// Transform a 3D direction by this matrix.
		Vector3 MultiplyDirection(const Vector3& v) const
		{
			const Reg128 result = MultiplyRows3(v.x, v.y, v.z);
			return Vector3(StoreLane<0>(result), StoreLane<1>(result), StoreLane<2>(result));
		}

		/// Transform a 4D vector by this matrix.
		/// @note If the matrix doesn't contain a projection, it's faster to use MultiplyAffine()
		Vector4 Multiply(const Vector4& v) const
		{
			const Reg128 result = SIMD::Add4(MultiplyRows3(v.x, v.y, v.z), SIMD::Mul4(SIMD::Set4(v.w), SIMD::Load4(m[3])));
			Vector4 output;
			SIMD::Store4(&output.x, result);
			return output;
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
		// The first three rows weighted by x, y and z and summed.
		Reg128 MultiplyRows3(float x, float y, float z) const
		{
			Reg128 result = SIMD::Mul4(SIMD::Set4(x), SIMD::Load4(m[0]));
			result = SIMD::Add4(result, SIMD::Mul4(SIMD::Set4(y), SIMD::Load4(m[1])));
			return SIMD::Add4(result, SIMD::Mul4(SIMD::Set4(z), SIMD::Load4(m[2])));
		}

		template<int Lane>
		static float StoreLane(Reg128 v)
		{
			return _mm_cvtss_f32(SIMD::Splat4<Lane>(v));
		}

		float m[4][4];
	};
}
