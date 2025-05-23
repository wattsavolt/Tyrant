

#include "Math/Matrix4.h"
#include "Math/Quaternion.h"

namespace tyr
{
	const Matrix4 Matrix4::c_Zero(ArgZero::Zero);
	const Matrix4 Matrix4::c_Identity(ArgIdentity::Identity);
	

	static float Minor(const Matrix4& m, const uint r0, const uint r1, const uint r2,
								const uint c0, const uint c1, const uint c2)
	{
		return m[r0][c0] * (m[r1][c1] * m[r2][c2] - m[r2][c1] * m[r1][c2]) -
			m[r0][c1] * (m[r1][c0] * m[r2][c2] - m[r2][c0] * m[r1][c2]) +
			m[r0][c2] * (m[r1][c0] * m[r2][c1] - m[r2][c0] * m[r1][c1]);
	}

	Matrix4 Matrix4::Adjoint() const
	{
		return Matrix4(Minor(*this, 1, 2, 3, 1, 2, 3),
			-Minor(*this, 0, 2, 3, 1, 2, 3),
			Minor(*this, 0, 1, 3, 1, 2, 3),
			-Minor(*this, 0, 1, 2, 1, 2, 3),

			-Minor(*this, 1, 2, 3, 0, 2, 3),
			Minor(*this, 0, 2, 3, 0, 2, 3),
			-Minor(*this, 0, 1, 3, 0, 2, 3),
			Minor(*this, 0, 1, 2, 0, 2, 3),

			Minor(*this, 1, 2, 3, 0, 1, 3),
			-Minor(*this, 0, 2, 3, 0, 1, 3),
			Minor(*this, 0, 1, 3, 0, 1, 3),
			-Minor(*this, 0, 1, 2, 0, 1, 3),

			-Minor(*this, 1, 2, 3, 0, 1, 2),
			Minor(*this, 0, 2, 3, 0, 1, 2),
			-Minor(*this, 0, 1, 3, 0, 1, 2),
			Minor(*this, 0, 1, 2, 0, 1, 2));
	}

	float Matrix4::Determinant() const
	{
		return m[0][0] * Minor(*this, 1, 2, 3, 1, 2, 3) -
			m[0][1] * Minor(*this, 1, 2, 3, 0, 2, 3) +
			m[0][2] * Minor(*this, 1, 2, 3, 0, 1, 3) -
			m[0][3] * Minor(*this, 1, 2, 3, 0, 1, 2);
	}

	float Matrix4::Determinant3x3() const
	{
		float cofactor00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
		float cofactor10 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
		float cofactor20 = m[1][0] * m[2][1] - m[1][1] * m[2][0];

		float det = m[0][0] * cofactor00 + m[0][1] * cofactor10 + m[0][2] * cofactor20;

		return det;
	}

	Matrix4 Matrix4::Inverse() const
	{
		float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
		float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
		float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
		float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

		float v0 = m20 * m31 - m21 * m30;
		float v1 = m20 * m32 - m22 * m30;
		float v2 = m20 * m33 - m23 * m30;
		float v3 = m21 * m32 - m22 * m31;
		float v4 = m21 * m33 - m23 * m31;
		float v5 = m22 * m33 - m23 * m32;

		float t00 = + (v5 * m11 - v4 * m12 + v3 * m13);
		float t10 = - (v5 * m10 - v2 * m12 + v1 * m13);
		float t20 = + (v4 * m10 - v2 * m11 + v0 * m13);
		float t30 = - (v3 * m10 - v1 * m11 + v0 * m12);

		float invDet = 1 / (t00 * m00 + t10 * m01 + t20 * m02 + t30 * m03);

		float d00 = t00 * invDet;
		float d10 = t10 * invDet;
		float d20 = t20 * invDet;
		float d30 = t30 * invDet;

		float d01 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
		float d11 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
		float d21 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
		float d31 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

		v0 = m10 * m31 - m11 * m30;
		v1 = m10 * m32 - m12 * m30;
		v2 = m10 * m33 - m13 * m30;
		v3 = m11 * m32 - m12 * m31;
		v4 = m11 * m33 - m13 * m31;
		v5 = m12 * m33 - m13 * m32;

		float d02 = + (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
		float d12 = - (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
		float d22 = + (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
		float d32 = - (v3 * m00 - v1 * m01 + v0 * m02) * invDet;

		v0 = m21 * m10 - m20 * m11;
		v1 = m22 * m10 - m20 * m12;
		v2 = m23 * m10 - m20 * m13;
		v3 = m22 * m11 - m21 * m12;
		v4 = m23 * m11 - m21 * m13;
		v5 = m23 * m12 - m22 * m13;

		float d03 = - (v5 * m01 - v4 * m02 + v3 * m03) * invDet;
		float d13 = + (v5 * m00 - v2 * m02 + v1 * m03) * invDet;
		float d23 = - (v4 * m00 - v2 * m01 + v0 * m03) * invDet;
		float d33 = + (v3 * m00 - v1 * m01 + v0 * m02) * invDet;
		
		return Matrix4(
			d00, d01, d02, d03,
			d10, d11, d12, d13,
			d20, d21, d22, d23,
			d30, d31, d32, d33);
	}

	Matrix4 Matrix4::InverseAffine() const
	{
		float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2];
		float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2];

		float t00 = m22 * m11 - m21 * m12;
		float t10 = m20 * m12 - m22 * m10;
		float t20 = m21 * m10 - m20 * m11;

		float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2];

		float invDet = 1 / (m00 * t00 + m01 * t10 + m02 * t20);

		t00 *= invDet; t10 *= invDet; t20 *= invDet;

		m00 *= invDet; m01 *= invDet; m02 *= invDet;

		float r00 = t00;
		float r01 = m02 * m21 - m01 * m22;
		float r02 = m01 * m12 - m02 * m11;

		float r10 = t10;
		float r11 = m00 * m22 - m02 * m20;
		float r12 = m02 * m10 - m00 * m12;

		float r20 = t20;
		float r21 = m01 * m20 - m00 * m21;
		float r22 = m00 * m11 - m01 * m10;

		float m03 = m[0][3], m13 = m[1][3], m23 = m[2][3];

		float r03 = - (r00 * m03 + r01 * m13 + r02 * m23);
		float r13 = - (r10 * m03 + r11 * m13 + r12 * m23);
		float r23 = - (r20 * m03 + r21 * m13 + r22 * m23);

		return Matrix4(
			r00, r01, r02, r03,
			r10, r11, r12, r13,
			r20, r21, r22, r23,
			  0,   0,   0,   1);
	}

	void Matrix4::SetTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		Matrix3 rot3x3;
		rotation.ToRotationMatrix(rot3x3);

		/*m[0][0] = scale.x * rot3x3[0][0]; m[0][1] = scale.y * rot3x3[0][1]; m[0][2] = scale.z * rot3x3[0][2]; m[0][3] = translation.x;
		m[1][0] = scale.x * rot3x3[1][0]; m[1][1] = scale.y * rot3x3[1][1]; m[1][2] = scale.z * rot3x3[1][2]; m[1][3] = translation.y;
		m[2][0] = scale.x * rot3x3[2][0]; m[2][1] = scale.y * rot3x3[2][1]; m[2][2] = scale.z * rot3x3[2][2]; m[2][3] = translation.z;
		m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;*/

		m[0][0] = scale.x * rot3x3[0][0];
		m[0][1] = scale.x * rot3x3[0][1]; 
		m[0][2] = scale.x * rot3x3[0][2]; 
		m[0][3] = 0;

		m[1][0] = scale.y * rot3x3[1][0];
		m[1][1] = scale.y * rot3x3[1][1];
		m[1][2] = scale.y * rot3x3[1][2];
		m[1][3] = 0;

		m[2][0] = scale.z * rot3x3[2][0];
		m[2][1] = scale.z * rot3x3[2][1];
		m[2][2] = scale.z * rot3x3[2][2];
		m[2][3] = 0;

		m[3][0] = translation.x; 
		m[3][1] = translation.y; 
		m[3][2] = translation.z; 
		m[3][3] = 1;
	}

	void Matrix4::SetInverseTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		// Invert the parameters
		Vector3 invTranslate = -translation;
		Vector3 invScale(1 / scale.x, 1 / scale.y, 1 / scale.z);
		Quaternion invRot = rotation.Inverse();

		// Because we're inverting, order is translation, rotation, scale
		// So make translation relative to scale & rotation
		invTranslate = invRot.Rotate(invTranslate);
		invTranslate *= invScale;

		// Next, make a 3x3 rotation matrix
		Matrix3 rot3x3;
		invRot.ToRotationMatrix(rot3x3);

		// Set up final matrix with scale, rotation and translation
		m[0][0] = invScale.x * rot3x3[0][0]; m[0][1] = invScale.x * rot3x3[0][1]; m[0][2] = invScale.x * rot3x3[0][2]; m[0][3] = 0;
		m[1][0] = invScale.y * rot3x3[1][0]; m[1][1] = invScale.y * rot3x3[1][1]; m[1][2] = invScale.y * rot3x3[1][2]; m[1][3] = 0;
		m[2][0] = invScale.z * rot3x3[2][0]; m[2][1] = invScale.z * rot3x3[2][1]; m[2][2] = invScale.z * rot3x3[2][2]; m[2][3] = 0;		
		m[3][0] = invTranslate.x;            m[3][1] = invTranslate.y;            m[3][2] = invTranslate.z;            m[3][3] = 1;
	}

	void Matrix4::LookAt(const Vector3& position, const Vector3& forward, const Vector3& up)
	{
		// View matrix form:
		//  [ Lx  Lx  Lx  0  ]
		//  [ Uy  Uy  Uy  0  ]
		//  [ Dz  Dz  Dz  0  ]
		//  [ Tx  Ty  Tz  1  ]
		// T = -(Transposed(Rot) * Pos)

		const Vector3 right = up.Cross(forward);
		// Most efficiently done using 3x3 Matrices.
		Matrix3 rot(right, up, forward);
		
		// Make the translation relative to new axes.
		Matrix3 rotTran = rot.Transpose();
		Vector3 trans = (-rotTran).Multiply(position);

		// Create final matrix.
		*this = Matrix4(rotTran);
		m[3][0] = trans.x;
		m[3][1] = trans.y;
		m[3][2] = trans.z;
	}

	void Matrix4::LookAt(const Vector3& position, const Quaternion& rotation)
	{
		// Most efficiently done using 3x3 Matrices.
		Matrix3 rot;
		rotation.ToRotationMatrix(rot);

		// Make the translation relative to new axes.
		Matrix3 rotTran = rot.Transpose();
		Vector3 trans = (-rotTran).Multiply(position);

		// Create final matrix.
		*this = Matrix4(rotTran);
		m[3][0] = trans.x;
		m[3][1] = trans.y;
		m[3][2] = trans.z;
	}

	void Matrix4::ToOrthographic(float left, float right, float top,
		float bottom, float near, float far)
	{
		// Creates a matrix that transforms a coordinate to a normalized device coordinate in the range:
		// Left -1 - Right 1
		// Bottom -1 - Top 1
		// Near -1 - Far 1

		float reciprocalWidth = 1.0f / (right - left);
		float reciprocalHeight = 1.0f / (top - bottom);
		float range = 1.0f / (near - far);

		m[0][0] = reciprocalWidth + reciprocalWidth;
		m[0][1] = 0.0f;
		m[0][2] = 0.0f;
		m[0][3] = 0.0f;

		m[1][0] = 0.0f;
		m[1][1] = reciprocalHeight + reciprocalHeight;
		m[1][2] = 0.0f;
		m[1][3] = 0.0f;

		m[2][0] = 0.0f;
		m[2][1] = 0.0f;
		m[2][2] = range;
		m[2][3] = 0.0f;

		m[3][0] = -(left + right) * reciprocalWidth;
		m[3][1] = -(top + bottom) * reciprocalHeight;
		m[3][2] = -range * near;
		m[3][3] = 1.0f;
	}

	Matrix4 Matrix4::CreateTranslation(const Vector3& translation)
	{
		Matrix4 mat;
		mat[0][0] = 1.0f;          mat[0][1] = 0.0f;          mat[0][2] = 0.0f;          mat[0][3] = 0.0f;
		mat[1][0] = 0.0f;          mat[1][1] = 1.0f;          mat[1][2] = 0.0f;          mat[1][3] = 0.0f;
		mat[2][0] = 0.0f;          mat[2][1] = 0.0f;          mat[2][2] = 1.0f;          mat[2][3] = 0.0f;
		mat[3][0] = translation.x; mat[3][1] = translation.y; mat[3][2] = translation.z; mat[3][3] = 1.0f;
		return mat;
	}

	Matrix4 Matrix4::CreateScaling(const Vector3& scale)
	{
		Matrix4 mat;

		mat[0][0] = scale.x; mat[0][1] = 0.0f; mat[0][2] = 0.0f; mat[0][3] = 0.0f;
		mat[1][0] = 0.0f; mat[1][1] = scale.y; mat[1][2] = 0.0f; mat[1][3] = 0.0f;
		mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = scale.z; mat[2][3] = 0.0f;
		mat[3][0] = 0.0f; mat[3][1] = 0.0f; mat[3][2] = 0.0f; mat[3][3] = 1.0f;

		return mat;
	}

	Matrix4 Matrix4::CreateScaling(float scale)
	{
		Matrix4 mat;

		mat[0][0] = scale; mat[0][1] = 0.0f; mat[0][2] = 0.0f; mat[0][3] = 0.0f;
		mat[1][0] = 0.0f; mat[1][1] = scale; mat[1][2] = 0.0f; mat[1][3] = 0.0f;
		mat[2][0] = 0.0f; mat[2][1] = 0.0f; mat[2][2] = scale; mat[2][3] = 0.0f;
		mat[3][0] = 0.0f; mat[3][1] = 0.0f; mat[3][2] = 0.0f; mat[3][3] = 1.0f;

		return mat;
	}

	Matrix4 Matrix4::CreateRotation(const Quaternion& rotation)
	{
		Matrix3 mat;
		rotation.ToRotationMatrix(mat);

		return Matrix4(mat);
	}

	Matrix4 Matrix4::CreatePerspective(float horzFOV, float aspect, float near, float far)
	{
		/*const float thetaX = Math::c_DegToRad * horzFOV * 0.5f;
		const float tanThetaX = Math::Tan(thetaX);
		const float invTanThetaX = 1 / tanThetaX;
		const float width = 1 / (tanThetaX * aspect);
		const float height = invTanThetaX;
		const float invD = 1.0f / (far - near);
		const float range = far * invD;
		const float zTrans = -range * near;*/

		TYR_ASSERT(near != far && near > 0 && far > 0);

		const float thetaX = Math::c_DegToRad * horzFOV * 0.5f;
		const float width = Math::Tan(thetaX);
		const float height = width * aspect;
		const float invD = 1.0f / (far - near);
		const float range = far * invD;
		const float zTrans = -range * near;

		Matrix4 mat;
		mat[0][0] = width; 
		mat[0][1] = 0.0f;   
		mat[0][2] = 0.0f;  
		mat[0][3] = 0.0f;

		mat[1][0] = 0.0f;  
		mat[1][1] = height; 
		mat[1][2] = 0.0f;   
		mat[1][3] = 0.0f;

		mat[2][0] = 0.0f;  
		mat[2][1] = 0.0f;   
		mat[2][2] = range;  
		mat[2][3] = 1.0f;

		mat[3][0] = 0.0f;  
		mat[3][1] = 0.0f;   
		mat[3][2] = zTrans; 
		mat[3][3] = 0.0f;

		return mat;
	}

	Matrix4 Matrix4::CreateOrthographic(float left, float right, float top, float bottom, float near, float far)
	{
		Matrix4 output;
		output.ToOrthographic(left, right, top, bottom, near, far);

		return output;
	}

	Matrix4 Matrix4::CreateView(const Vector3& position, const Vector3& forward, const Vector3& up)
	{
		Matrix4 mat;
		mat.LookAt(position, forward, up);

		return mat;
	}

	Matrix4 Matrix4::CreateView(const Vector3& position, const Quaternion& rotation)
	{
		Matrix4 mat;
		mat.LookAt(position, rotation);

		return mat;
	}

	Matrix4 Matrix4::CreateTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		Matrix4 mat;
		mat.SetTRS(translation, rotation, scale);

		return mat;
	}

	Matrix4 Matrix4::CreateInverseTRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		Matrix4 mat;
		mat.SetInverseTRS(translation, rotation, scale);

		return mat;
	}
}
