#ifndef YIALITE_STD_CONTAINERS_H
#define YIALITE_STD_CONTAINERS_H

#include "../memory/std_allocator.h"

#include <deque>
#include <functional>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace yialite
{

template <typename T>
using StdVector = std::vector<T, Allocator<T>>;

template <typename T>
using StdDeque = std::deque<T, Allocator<T>>;

template <typename T>
using StdList = std::list<T, Allocator<T>>;

template <typename K, typename V, typename Less = std::less<K>>
using StdMap = std::map<K, V, Less, Allocator<std::pair<const K, V>>>;

template <typename K, typename V, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
using StdUnorderedMap = std::unordered_map<K, V, Hash, Equal, Allocator<std::pair<const K, V>>>;

template <typename K, typename Hash = std::hash<K>, typename Equal = std::equal_to<K>>
using StdUnorderedSet = std::unordered_set<K, Hash, Equal, Allocator<K>>;

using StdString = std::basic_string<char, std::char_traits<char>, Allocator<char>>;
using StdStringView = std::string_view;

} // namespace yialite

#endif // YIALITE_STD_CONTAINERS_H
