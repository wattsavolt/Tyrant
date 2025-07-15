#pragma once

// std containers 
#include <map>
#include <bitset>
#include <set>
#include <forward_list>
#include <unordered_map>
#include <unordered_set>
#include <array>

#include "FixedArray.h"
#include "Array.h"
#include "LocalArray.h"
#include "HashMap.h"
#include "SPSCRingBuffer.h"
#include "MPSCRingBuffer.h"

namespace tyr
{
	template <typename T, typename P = std::less<T>>
	using Set = std::set<T, P>;

	template <typename K, typename V, typename P = std::less<K>>
	using Map = std::map<K, V, P>;

	template <typename T, typename P = std::less<T>>
	using MultiSet = std::multiset<T, P>;

	template <typename K, typename V, typename P = std::less<K>>
	using MultiMap = std::multimap<K, V, P>;

	template <typename T, typename H, typename C = std::equal_to<T>>
	using UnorderedSet = std::unordered_set<T, H, C>;

	template <typename K, typename V, typename H = std::hash<K>, typename C = std::equal_to<K>>
	using UnorderedMap = std::unordered_map<K, V, H, C>;

	template <typename K, typename V, typename H, typename C = std::equal_to<K>>
	using UnorderedMultiMap = std::unordered_multimap<K, V, H, C>;
}

