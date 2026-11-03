#pragma once

#include "system/Bedrock/Assert.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace CarPlay::System::Bedrock::Storage {

/** @brief Checks whether value is a nonzero power of two, which every alignment below must be. */
[[nodiscard]] constexpr bool IsPowerOfTwo(std::size_t value) noexcept {
  return std::has_single_bit(value);
}

/** @brief Returns the address behind pointer as a number, so the helpers below work on it. */
[[nodiscard]] inline std::size_t AddressValue(const void* pointer) noexcept {
  return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

/** @brief Checks whether value is a multiple of alignment. */
[[nodiscard]] constexpr bool IsOnBoundary(std::size_t value, std::size_t alignment) noexcept {
  CARPLAY_ASSERT(IsPowerOfTwo(alignment), "alignment must be a power of two");
  return (value & (alignment - 1)) == 0;
}

/** @brief Returns the bytes that take value to the next multiple of alignment, zero when it is one already; never overflows. */
[[nodiscard]] constexpr std::size_t PaddingToBoundary(std::size_t value, std::size_t alignment) noexcept {
  CARPLAY_ASSERT(IsPowerOfTwo(alignment), "alignment must be a power of two");
  return (alignment - (value & (alignment - 1))) & (alignment - 1);
}

/** @brief Rounds value down to a multiple of alignment. */
[[nodiscard]] constexpr std::size_t RoundDownToBoundary(std::size_t value, std::size_t alignment) noexcept {
  CARPLAY_ASSERT(IsPowerOfTwo(alignment), "alignment must be a power of two");
  return value & ~(alignment - 1);
}

/**
 * @brief Rounds value up to a multiple of alignment.
 * @details The result must fit into std::size_t; values that come from outside go through TryRoundUpToBoundary.
 */
[[nodiscard]] constexpr std::size_t RoundUpToBoundary(std::size_t value, std::size_t alignment) noexcept {
  CARPLAY_ASSERT(IsPowerOfTwo(alignment), "alignment must be a power of two");
  CARPLAY_ASSERT(value <= (std::numeric_limits<std::size_t>::max)() - (alignment - 1), "rounding up overflows");
  return (value + (alignment - 1)) & ~(alignment - 1);
}

/** @brief Rounds value up to a multiple of alignment; returns false and leaves rounded untouched when the result does not fit. */
[[nodiscard]] constexpr bool TryRoundUpToBoundary(std::size_t value, std::size_t alignment, std::size_t& rounded) noexcept {
  CARPLAY_ASSERT(IsPowerOfTwo(alignment), "alignment must be a power of two");
  const std::size_t mask = alignment - 1;
  if (value > (std::numeric_limits<std::size_t>::max)() - mask) {
    return false;
  }
  rounded = (value + mask) & ~mask;
  return true;
}

/** @brief Divides and rounds up; denominator must not be zero. */
[[nodiscard]] constexpr std::size_t CeilDivide(std::size_t numerator, std::size_t denominator) noexcept {
  CARPLAY_ASSERT(denominator != 0, "division by zero");
  return numerator / denominator + (numerator % denominator != 0 ? 1 : 0);
}

}  // namespace CarPlay::System::Bedrock::Storage