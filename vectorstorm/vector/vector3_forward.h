#pragma once

#ifdef VECTORSTORM_NAMESPACE
namespace VECTORSTORM_NAMESPACE {
#endif // VECTORSTORM_NAMESPACE

template<typename T> class vector3;

#ifdef VECTORSTORM_NAMESPACE
}
#endif // VECTORSTORM_NAMESPACE

#include "vector3_types.h"

namespace std {

template<typename T> constexpr vector3<T> min(vector3<T> const &a, vector3<T> const &b) noexcept;
template<typename T> constexpr vector3<T> max(vector3<T> const &a, vector3<T> const &b) noexcept;

}
