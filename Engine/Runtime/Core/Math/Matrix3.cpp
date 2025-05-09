
#include "Math/Matrix3.h"
#include "Math/Math.h"
#include "Math/Quaternion.h"

namespace tyr
{
	const Matrix3 Matrix3::c_Zero(ArgZero::Zero);
	const Matrix3 Matrix3::c_Identity(ArgIdentity::Identity);

	Vector3 Matrix3::GetRow(uint row) const
	{
		TYR_ASSERT(row < 3);

		return Vector3(m[row][0], m[row][1], m[row][2]);
	}

	void Matrix3::SetRow(uint row, const Vector3& vec)
	{
		TYR_ASSERT(row < 3);

		m[row][0] = vec.x;
		m[row][1] = vec.y;
		m[row][2] = vec.z;
	}

	Vector3 Matrix3::GetColumn(uint col) const
	{
		TYR_ASSERT(col < 3);

		return Vector3(m[0][col],m[1][col], m[2][col]);
	}

	void Matrix3::SetColumn(uint col, const Vector3& vec)
	{
		TYR_ASSERT(col < 3);

		m[0][col] = vec.x;
		m[1][col] = vec.y;
		m[2][col] = vec.z;
	}

	void Matrix3::FromAxes(const Vector3& xAxis, const Vector3& yAxis, const Vector3& zAxis)
	{
		SetRow(0, xAxis);
		SetRow(1, yAxis);
		SetRow(2, zAxis);
	}

	bool Matrix3::operator== (const Matrix3& rhs) const
	{
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
			{
				if (m[row][col] != rhs.m[row][col])
				{
					return false;
				}
			}
		}

		return true;
	}

	bool Matrix3::operator!= (const Matrix3& rhs) const
	{
		return !operator==(rhs);
	}

	Matrix3 Matrix3::operator+ (const Matrix3& rhs) const
	{
		Matrix3 sum;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
			{
				sum.m[row][col] = m[row][col] + rhs.m[row][col];
			}
		}

		return sum;
	}

	Matrix3 Matrix3::operator- (const Matrix3& rhs) const
	{
		Matrix3 diff;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
			{
				diff.m[row][col] = m[row][col] -
					rhs.m[row][col];
			}
		}

		return diff;
	}

	Matrix3 Matrix3::operator* (const Matrix3& rhs) const
	{
		Matrix3 prod;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
			{
				prod.m[row][col] = 
					m[row][0] * rhs.m[0][col] +
					m[row][1] * rhs.m[1][col] + 
					m[row][2] * rhs.m[2][col];
			}
		}

		return prod;
	}

	Matrix3 Matrix3::operator- () const
	{
		Matrix3 neg;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
				neg[row][col] = -m[row][col];
		}

		return neg;
	}

	Matrix3 Matrix3::operator* (float rhs) const
	{
		Matrix3 prod;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
				prod[row][col] = rhs*m[row][col];
		}

		return prod;
	}

	Matrix3 operator* (float lhs, const Matrix3& rhs)
	{
		Matrix3 prod;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
				prod[row][col] = lhs*rhs.m[row][col];
		}

		return prod;
	}

	Vector3 Matrix3::Multiply(const Vector3& vec) const
	{
		Vector3 output;
		for (uint i = 0; i < 3; ++i)
		{
			output[i] =
				vec[0] * m[0][i] +
				vec[1] * m[1][i] +
				vec[2] * m[2][i];
		}

		return output;
	}

	Matrix3 Matrix3::Transpose() const
	{
		Matrix3 t;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
				t[row][col] = m[col][row];
		}

		return t;
	}

	bool Matrix3::Inverse(Matrix3& matInv, float tolerance) const
	{
		matInv[0][0] = m[1][1]*m[2][2] - m[1][2]*m[2][1];
		matInv[0][1] = m[0][2]*m[2][1] - m[0][1]*m[2][2];
		matInv[0][2] = m[0][1]*m[1][2] - m[0][2]*m[1][1];
		matInv[1][0] = m[1][2]*m[2][0] - m[1][0]*m[2][2];
		matInv[1][1] = m[0][0]*m[2][2] - m[0][2]*m[2][0];
		matInv[1][2] = m[0][2]*m[1][0] - m[0][0]*m[1][2];
		matInv[2][0] = m[1][0]*m[2][1] - m[1][1]*m[2][0];
		matInv[2][1] = m[0][1]*m[2][0] - m[0][0]*m[2][1];
		matInv[2][2] = m[0][0]*m[1][1] - m[0][1]*m[1][0];

		float det = m[0][0]*matInv[0][0] + m[0][1]*matInv[1][0] + m[0][2]*matInv[2][0];

		if (Math::Abs(det) <= tolerance)
		{
			return false;
		}

		float invDet = 1.0f/det;
		for (uint row = 0; row < 3; row++)
		{
			for (uint col = 0; col < 3; col++)
				matInv[row][col] *= invDet;
		}

		return true;
	}

	Matrix3 Matrix3::Inverse(float tolerance) const
	{
		Matrix3 inv = Matrix3::c_Zero;
		Inverse(inv, tolerance);
		return inv;
	}

	float Matrix3::Determinant() const
	{
		float cofactor00 = m[1][1]*m[2][2] - m[1][2]*m[2][1];
		float cofactor10 = m[1][2]*m[2][0] - m[1][0]*m[2][2];
		float cofactor20 = m[1][0]*m[2][1] - m[1][1]*m[2][0];

		float det = m[0][0]*cofactor00 + m[0][1]*cofactor10 + m[0][2]*cofactor20;

		return det;
	}

	void Matrix3::Orthonormalize()
	{
		// Compute q0
		float invLength = Math::InvSqrt(m[0][0]*m[0][0]+ m[1][0]*m[1][0] + m[2][0]*m[2][0]);

		m[0][0] *= invLength;
		m[1][0] *= invLength;
		m[2][0] *= invLength;

		// Compute q1
		float dot0 = m[0][0]*m[0][1] + m[1][0]*m[1][1] + m[2][0]*m[2][1];

		m[0][1] -= dot0*m[0][0];
		m[1][1] -= dot0*m[1][0];
		m[2][1] -= dot0*m[2][0];

		invLength = Math::InvSqrt(m[0][1]*m[0][1] + m[1][1]*m[1][1] + m[2][1]*m[2][1]);

		m[0][1] *= invLength;
		m[1][1] *= invLength;
		m[2][1] *= invLength;

		// Compute q2
		float dot1 = m[0][1]*m[0][2] + m[1][1]*m[1][2] + m[2][1]*m[2][2];
		dot0 = m[0][0]*m[0][2] + m[1][0]*m[1][2] + m[2][0]*m[2][2];

		m[0][2] -= dot0*m[0][0] + dot1*m[0][1];
		m[1][2] -= dot0*m[1][0] + dot1*m[1][1];
		m[2][2] -= dot0*m[2][0] + dot1*m[2][1];

		invLength = Math::InvSqrt(m[0][2]*m[0][2] + m[1][2]*m[1][2] + m[2][2]*m[2][2]);

		m[0][2] *= invLength;
		m[1][2] *= invLength;
		m[2][2] *= invLength;
	}

	void Matrix3::ToAxisAngle(Vector3& axis, float& radians) const
	{
		float trace = m[0][0] + m[1][1] + m[2][2];
		float cos = 0.5f*(trace-1.0f);
		radians = Math::Acos(cos);  // In [0, Pi]

		if (radians > 0.0f)
		{
			if (radians < Math::c_Pi)
			{
				axis.x = m[2][1]-m[1][2];
				axis.y = m[0][2]-m[2][0];
				axis.z = m[1][0]-m[0][1];
				axis.SafeNormalize();
			}
			else
			{
				// Angle is Pi
				float fHalfInverse;
				if (m[0][0] >= m[1][1])
				{
					// r00 >= r11
					if (m[0][0] >= m[2][2])
					{
						// r00 is maximum diagonal term
						axis.x = 0.5f*Math::Sqrt(m[0][0] - m[1][1] - m[2][2] + 1.0f);
						fHalfInverse = 0.5f/axis.x;
						axis.y = fHalfInverse*m[0][1];
						axis.z = fHalfInverse*m[0][2];
					}
					else
					{
						// r22 is maximum diagonal term
						axis.z = 0.5f*Math::Sqrt(m[2][2] - m[0][0] - m[1][1] + 1.0f);
						fHalfInverse = 0.5f/axis.z;
						axis.x = fHalfInverse*m[0][2];
						axis.y = fHalfInverse*m[1][2];
					}
				}
				else
				{
					// r11 > r00
					if ( m[1][1] >= m[2][2] )
					{
						// r11 is maximum diagonal term
						axis.y = 0.5f*Math::Sqrt(m[1][1] - m[0][0] - m[2][2] + 1.0f);
						fHalfInverse  = 0.5f/axis.y;
						axis.x = fHalfInverse*m[0][1];
						axis.z = fHalfInverse*m[1][2];
					}
					else
					{
						// r22 is maximum diagonal term
						axis.z = 0.5f*Math::Sqrt(m[2][2] - m[0][0] - m[1][1] + 1.0f);
						fHalfInverse = 0.5f/axis.z;
						axis.x = fHalfInverse*m[0][2];
						axis.y = fHalfInverse*m[1][2];
					}
				}
			}
		}
		else
		{
			// The angle is 0 and the matrix is the identity.  Any axis will
			// work, so just use the x-axis.
			axis.x = 1.0f;
			axis.y = 0.0f;
			axis.z = 0.0f;
		}
	}

	void Matrix3::FromAxisAngle(const Vector3& axis, const float angle)
	{
		float cos = Math::Cos(angle);
		float sin = Math::Sin(angle);
		float oneMinusCos = 1.0f-cos;
		float x2 = axis.x*axis.x;
		float y2 = axis.y*axis.y;
		float z2 = axis.z*axis.z;
		float xym = axis.x*axis.y*oneMinusCos;
		float xzm = axis.x*axis.z*oneMinusCos;
		float yzm = axis.y*axis.z*oneMinusCos;
		float xSin = axis.x*sin;
		float ySin = axis.y*sin;
		float zSin = axis.z*sin;

		/*m[0][0] = x2*oneMinusCos+cos;
		m[0][1] = xym-zSin;
		m[0][2] = xzm+ySin;
		m[1][0] = xym+zSin;
		m[1][1] = y2*oneMinusCos+cos;
		m[1][2] = yzm-xSin;
		m[2][0] = xzm-ySin;
		m[2][1] = yzm+xSin;
		m[2][2] = z2*oneMinusCos+cos;*/

		m[0][0] = x2 * oneMinusCos + cos;
		m[0][1] = xym + zSin;
		m[0][2] = xzm - ySin;
		m[1][0] = xym - zSin;
		m[1][1] = y2 * oneMinusCos + cos;
		m[1][2] = yzm + xSin;
		m[2][0] = xzm + ySin;
		m[2][1] = yzm - xSin;
		m[2][2] = z2 * oneMinusCos + cos;
	}

	void Matrix3::ToQuaternion(Quaternion& quat) const
	{
		quat.FromRotationMatrix(*this);
	}

	void Matrix3::FromQuaternion(const Quaternion& quat)
	{
		quat.ToRotationMatrix(*this);
	}

	bool Matrix3::ToEulerAngles(float& xAngle, float& yAngle, float& zAngle) const
	{
		float m21 = m[2][1];
		if (m21 < 1)
		{
			if (m21 > -1)
			{
				xAngle = Math::Asin(m21);
				yAngle = Math::Atan2(-m[2][0], m[2][2]);
				zAngle = Math::Atan2(-m[0][1], m[1][1]);

				return true;
			}
			else
			{
				// Note: Not an unique solution.
				xAngle = -Math::c_HalfPi;
				yAngle = 0.0f;
				zAngle = -Math::Atan2(m[0][2], m[0][0]);

				return false;
			}
		}
		else
		{
			// Note: Not an unique solution.
			xAngle = Math::c_HalfPi;
			yAngle = 0.0f;
			zAngle = Math::Atan2(m[0][2], m[0][0]);
			
			return false;
		}
	}

	void Matrix3::FromEulerAngles(const float xAngle, const float yAngle, const float zAngle)
	{
		float cx = Math::Cos(xAngle);
		float sx = Math::Sin(xAngle);

		float cy = Math::Cos(yAngle);
		float sy = Math::Sin(yAngle);

		float cz = Math::Cos(zAngle);
		float sz = Math::Sin(zAngle);

		m[0][0] = cy * cz - sx * sy * sz;
		m[0][1] = -cx * sz;
		m[0][2] = cz * sy + cy * sx * sz;

		m[1][0] = cz * sx * sy + cy * sz;
		m[1][1] = cx * cz;
		m[1][2] = -cy * cz * sx + sy * sz;

		m[2][0] = -cx * sy;
		m[2][1] = sx;
		m[2][2] = cx * cy;
	}

	void Matrix3::FromEulerAngles(const float xAngle, const float yAngle, const float zAngle, AxesOrder order)
	{
		// Euler angle conversions
		static constexpr const EulerAngleOrderData orders[6] =
		{ 
		  { 0, 1, 2, 1.0f}, { 0, 2, 1, -1.0f}, { 1, 0, 2, -1.0f},
		  { 1, 2, 0, 1.0f}, { 2, 0, 1,  1.0f}, { 2, 1, 0, -1.0f} 
		};

		const auto& l = orders[(int)order];

		Matrix3 mats[3];
		float cx = Math::Cos(xAngle);
		float sx = Math::Sin(xAngle);
		mats[0] = Matrix3
		(
			1.0f, 0.0f, 0.0f,
			0.0f, cx, -sx,
			0.0f, sx, cx
		);

		float cy = Math::Cos(yAngle);
		float sy = Math::Sin(yAngle);
		mats[1] = Matrix3
		(
			cy, 0.0f, sy,
			0.0f, 1.0f, 0.0f,
			-sy, 0.0f, cy
		);

		float cz = Math::Cos(zAngle);
		float sz = Math::Sin(zAngle);
		mats[2] = Matrix3
		(
			cz, -sz, 0.0f,
			sz, cz, 0.0f,
			0.0f, 0.0f, 1.0f
		);

		*this = mats[l.c]*(mats[l.b]*mats[l.a]);
	}
}
