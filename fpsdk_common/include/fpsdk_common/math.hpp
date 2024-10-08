/**
 * \verbatim
 * ___    ___
 * \  \  /  /
 *  \  \/  /   Copyright (c) Fixposition AG (www.fixposition.com) and contributors
 *  /  /\  \   License: see the LICENSE file
 * /__/  \__\
 * \endverbatim
 *
 * @file
 * @brief Fixposition SDK: Math utilities
 *
 * @page FPSDK_COMMON_MATH Math utilities
 *
 * **API**: fpsdk_common/math.hpp and fpsdk::common::math
 *
 */
#ifndef __FPSDK_COMMON_MATH_HPP__
#define __FPSDK_COMMON_MATH_HPP__

/* LIBC/STL */
#include <algorithm>
#include <cmath>
#include <cstdint>

/* EXTERNAL */

/* PACKAGE */

namespace fpsdk {
namespace common {
/**
 * @brief Math utilities
 */
namespace math {
/* ****************************************************************************************************************** */

/**
 * @brief Clamp value in range
 *
 * @tparam T numeric type
 * @param[in]  val  The value
 * @param[in]  min  Minimum value
 * @param[in]  max  Maximum value
 *
 * @note c++-17 has std::clamp() doing exactly (?) this...
 *
 * @returns the value clamped to the given range
 */
template <typename T>
constexpr T Clamp(const T val, const T min, const T max)
{
    return std::max(min, std::min(val, max));
}

/**
 * @brief Convert degrees to radians
 *
 * @tparam  T  value type
 * @param[in] degrees  Angle in degrees
 *
 * @returns the angle in radians
 */
template <typename T>
constexpr inline T DegToRad(T degrees)
{
    static_assert(::std::is_floating_point<T>::value, "Value must be float or double");
    return degrees * M_PI / 180.0;
}

/**
 * @brief Convert radians to degrees
 *
 * @tparam  T  value type
 * @param[in] radians  Angle in radians
 * @returns the angle in radians
 */
template <typename T>
constexpr inline T RadToDeg(T radians)
{
    static_assert(::std::is_floating_point<T>::value, "Value must be float or double");
    return radians * 180.0 / M_PI;
}

/**
 * @brief Round to desired number of fractional digits (of precision)
 *
 * @param[in]  value   The value
 * @param[in]  digits  Number of digits (0-12), param clamped to range
 *
 * @returns the value rounded to the given number of fractional digits, or the original value if it is not finite
 */
double RoundToFracDigits(const double value, const int digits);

/* ****************************************************************************************************************** */
}  // namespace math
}  // namespace common
}  // namespace fpsdk
#endif  // __FPSDK_COMMON_MATH_HPP__
