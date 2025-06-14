#pragma once

// std containers 
#include <stack>
#include <map>
#include <deque>
#include <queue>
#include <bitset>
#include <set>
#include <list>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <array>

#include "Array.h"
#include "LocalArray.h"
#include "FixedHashMap.h"

namespace tyr
{
	template <typename T>
	using Deque = std::deque<T>;

	template <typename T>
	using List = std::list<T>;

	template <typename T>
	using ForwardList = std::forward_list<T>;

	template <typename T>
	using Stack = std::stack<T, std::deque<T>>;

	template <typename T>
	using Queue = std::queue<T>;

	template <typename T, typename P = std::less<T>>
	using Set = std::set<T, P>;

	template <typename K, typename V, typename P = std::less<K>>
	using Map = std::map<K, V, P>;

	template <typename T, typename P = std::less<T>>
	using MultiSet = std::multiset<T, P>;

	template <typename K, typename V, typename P = std::less<K>>
	using MultiMap = std::multimap<K, V, P>;

	template <typename T, typename H, typename C = std::equal_to<T>>
	using HashSet = std::unordered_set<T, H, C>;

	template <typename K, typename V, typename H = std::hash<K>, typename C = std::equal_to<K>>
	using HashMap = std::unordered_map<K, V, H, C>;

	template <typename K, typename V, typename H, typename C = std::equal_to<K>>
	using HashMultiMap = std::unordered_multimap<K, V, H, C>;

	template<typename T, std::size_t N>
	using FixedArray = std::array<T, N>;
}

