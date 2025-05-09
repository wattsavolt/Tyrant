
#pragma once

#include <memory>

#include "Memory/Allocation.h"

namespace tyr
{
	template <typename T>
	using URef = std::unique_ptr<T>;

	template<typename Type>
	URef<Type> CreateURef(Type* data)
	{
		return std::unique_ptr<Type>(data);
	}

	template<typename T, typename... Args>
	constexpr URef<T> MakeURef(Args&&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	using Ref = std::shared_ptr<T>;

	template<typename Type>
	Ref<Type> CreateRef(Type* data)
	{
		return std::shared_ptr<Type>(data);
	}

	template<typename T, typename... Args>
	constexpr Ref<T> MakeRef(Args&&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	template <typename T>
	using WRef = std::weak_ptr<T>;
}

