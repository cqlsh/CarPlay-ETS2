#pragma once

#include "system/Bedrock/Target/Toolchain.hpp"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace CarPlay::System::Bedrock {

/** @brief Accepts byte-sized fault enumerations whose None enumerator stands for success. */
template <typename Fault>
concept FaultEnum = std::is_enum_v<Fault> &&
    std::is_same_v<std::underlying_type_t<Fault>, std::uint8_t> &&
    requires {
      { Fault::None } -> std::same_as<Fault>;
    };

/** @brief Checks whether a fault value reports success. */
template <FaultEnum Fault>
[[nodiscard]] constexpr bool IsSuccess(Fault fault) noexcept {
  return fault == Fault::None;
}

/** @brief Checks whether a fault value reports a failure. */
template <FaultEnum Fault>
[[nodiscard]] constexpr bool IsFailure(Fault fault) noexcept {
  return fault != Fault::None;
}

}  // namespace CarPlay::System::Bedrock