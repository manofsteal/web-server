#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <deque>
#include <array>
#include <queue>
#include <stack>
#include <functional>
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream>

namespace websrv {

// Standard STL containers
template<typename T>
using Vector = std::vector<T>;

template<typename K, typename V>
using Map = std::map<K, V>;

template<typename K, typename V>
using HashMap = std::unordered_map<K, V>;

template<typename T>
using Set = std::set<T>;

template<typename T>
using HashSet = std::unordered_set<T>;

template<typename T>
using List = std::list<T>;

template<typename T>
using Deque = std::deque<T>;

template<typename T, size_t N>
using Array = std::array<T, N>;

template<typename T>
using Queue = std::queue<T>;

template<typename T>
using Stack = std::stack<T>;

// String types
using String = std::string;
using StringStream = std::stringstream;
using IStringStream = std::istringstream;
using OStringStream = std::ostringstream;

// Smart pointers
template<typename T>
using UniquePtr = std::unique_ptr<T>;

template<typename T>
using SharedPtr = std::shared_ptr<T>;

template<typename T>
using WeakPtr = std::weak_ptr<T>;

// Function types
template<typename T>
using Function = std::function<T>;

// I/O types
using IStream = std::istream;
using OStream = std::ostream;
using IOStream = std::iostream;
using IFStream = std::ifstream;
using OFStream = std::ofstream;
using FStream = std::fstream;

// Common type aliases for networking
template<typename T>
using StringMap = Map<String, T>;

// Convenience functions for container creation
template<typename T>
inline Vector<T> make_vector() {
    return Vector<T>();
}

template<typename T>
inline Vector<T> make_vector(size_t size) {
    Vector<T> vec;
    vec.reserve(size);
    return vec;
}

template<typename K, typename V>
inline Map<K, V> make_map() {
    return Map<K, V>();
}

template<typename K, typename V>
inline HashMap<K, V> make_hashmap() {
    return HashMap<K, V>();
}

template<typename K, typename V>
inline HashMap<K, V> make_hashmap(size_t bucket_count) {
    return HashMap<K, V>(bucket_count);
}

template<typename T>
inline Set<T> make_set() {
    return Set<T>();
}

inline String make_string() {
    return String();
}

inline String make_string(const char* str) {
    return String(str);
}

inline String make_string(size_t capacity) {
    String str;
    str.reserve(capacity);
    return str;
}

template<typename T>
inline StringMap<T> make_string_map() {
    return StringMap<T>();
}

// Make functions for smart pointers
template<typename T>
inline UniquePtr<T> MakeUnique(T&& value) {
    return std::make_unique<T>(std::forward<T>(value));
}

template<typename T, typename... Args>
inline UniquePtr<T> MakeUnique(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T>
inline SharedPtr<T> MakeShared(T&& value) {
    return std::make_shared<T>(std::forward<T>(value));
}

template<typename T, typename... Args>
inline SharedPtr<T> MakeShared(Args&&... args) {
    return std::make_shared<T>(std::forward<Args>(args)...);
}

} // namespace websrv
