
#pragma once

#include "Memory/FrameAllocation.h"

namespace tyr
{
	template <typename T, typename A = StdAllocator<T, FrameAllocator>>
	using FrameVector = std::vector<T, A>;

	template <typename T, typename A = StdAllocator<T, FrameAllocator>>
	using FrameStack = std::stack<T, std::deque<T, A>>;

	template <typename T, typename A = StdAllocator<T, FrameAllocator>>
	using FrameQueue = std::queue<T, std::deque<T, A>>;

	template <typename T, typename P = std::less<T>, typename A = StdAllocator<T, FrameAllocator>>
	using FrameSet = std::set <T, P, A>;

	template <typename K, typename V, typename P = std::less<K>, typename A = StdAllocator<std::pair<const K, V>, FrameAllocator>>
	using FrameMap = std::map <K, V, P, A>;

	template <typename T, typename H = std::hash<T>, typename C = std::equal_to<T>, typename A = StdAllocator<T, FrameAllocator>>
	using FrameUnorderedSet = std::unordered_set<T, H, C, A>;

	template <typename K, typename V, typename H = std::hash<K>, typename C = std::equal_to<K>, typename A = StdAllocator<std::pair<const K, V>, FrameAllocator>>
	using FrameUnorderedMap = std::unordered_map<K, V, H, C, A>;

	template <typename T, typename A>
	using FrameArray = Array<T, FrameAllocator>;
}

