#pragma once

#include <cstddef>
#include <functional>

#ifdef VECTORSTORM_NO_BOOST
#ifdef VECTORSTORM_NAMESPACE
namespace VECTORSTORM_NAMESPACE {
#else
namespace vectorstorm_internal {
#endif // VECTORSTORM_NAMESPACE

template<typename T>
inline constexpr void hash_combine(size_t &seed, T const &v) {
  /// Reimplementation of boost's hash_combine
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

#ifdef VECTORSTORM_NAMESPACE
} // namespace VECTORSTORM_NAMESPACE
  #define HASH_COMBINE VECTORSTORM_NAMESPACE::hash_combine
#else
} // namespace vectorstorm_internal
  #define HASH_COMBINE vectorstorm_internal::hash_combine
#endif // VECTORSTORM_NAMESPACE
#else
  #define HASH_COMBINE boost::hash_combine
#endif // VECTORSTORM_NO_BOOST
