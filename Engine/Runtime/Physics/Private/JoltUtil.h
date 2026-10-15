#pragma once

#include "PhysicsTypes.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace tyr
{
	// Converts between the library's types and Jolt's. Only used inside this library.
	namespace JoltUtil
	{
		inline JPH::Vec3 ToJolt(const Vector3& v)
		{
			return JPH::Vec3(v.x, v.y, v.z);
		}

		inline JPH::Quat ToJolt(const Quaternion& q)
		{
			return JPH::Quat(q.x, q.y, q.z, q.w);
		}

		inline Vector3 FromJolt(JPH::Vec3Arg v)
		{
			return Vector3(v.GetX(), v.GetY(), v.GetZ());
		}

		inline Quaternion FromJolt(JPH::QuatArg q)
		{
			return Quaternion(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
		}

		// A PhysicsShape is a Jolt shape under another name.
		inline const JPH::Shape* ToJolt(const PhysicsShape* shape)
		{
			return reinterpret_cast<const JPH::Shape*>(shape);
		}

		inline const PhysicsShape* FromJolt(const JPH::Shape* shape)
		{
			return reinterpret_cast<const PhysicsShape*>(shape);
		}

		// Hands the result's shape out with a reference held, or null if it failed.
		inline const PhysicsShape* TakeShape(const JPH::ShapeSettings::ShapeResult& result, const char* what)
		{
			if (result.HasError())
			{
				TYR_LOG_ERROR("Couldn't make %s: %s", what, result.GetError().c_str());
				return nullptr;
			}
			const JPH::Shape* shape = result.Get().GetPtr();
			shape->AddRef();
			return FromJolt(shape);
		}
	}
}
