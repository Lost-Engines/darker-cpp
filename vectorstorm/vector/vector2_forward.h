#pragma once

#ifdef VECTORSTORM_NAMESPACE
namespace VECTORSTORM_NAMESPACE {
#endif // VECTORSTORM_NAMESPACE

template<typename T> class vector2;

#ifdef VECTORSTORM_NAMESPACE
}
#endif // VECTORSTORM_NAMESPACE

#include "vector2_types.h"

namespace std {

template<typename T> constexpr vector2<T> min(vector2<T> const &a, vector2<T> const &b) noexcept;
template<typename T> constexpr vector2<T> max(vector2<T> const &a, vector2<T> const &b) noexcept;

}
