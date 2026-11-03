#pragma once

#include "system/Bedrock/Target/Toolchain.hpp"

#include <source_location>

namespace CarPlay::System::Bedrock {

/** @brief Receives every assertion failure before the process is terminated; message may be nullptr. */
using AssertionSink = void (*)(const char* expression, const char* message, const std::source_location& location) noexcept;

/**
 * @brief Installs the sink that sees assertion failures first, or removes it with nullptr.
 * @details Stored with relaxed atomics; install it before worker threads start.
 */
void SetAssertionSink(AssertionSink sink) noexcept;

/** @brief Returns the installed sink, or nullptr. */
[[nodiscard]] AssertionSink GetAssertionSink() noexcept;

/**
 * @brief Reports a failed assertion to the sink, the debugger output and stderr, then terminates the process.
 * @details Breaks into an attached debugger before terminating. A sink may terminate the process itself.
 */
[[noreturn]] void AssertionFailed(const char* expression, const char* message = nullptr, std::source_location location = std::source_location::current()) noexcept;

}  // namespace CarPlay::System::Bedrock

/**
 * @brief Checks an internal invariant in debug builds, with an optional const char* message.
 * @details Release builds evaluate neither the condition nor the message, but still type-check the condition.
 *          Side effects and validation of external input must stay outside assertions.
 */
#if CARPLAY_DEBUG_BUILD
#define CARPLAY_ASSERT(condition, ...) \
  static_cast<void>(static_cast<bool>(condition) || (::CarPlay::System::Bedrock::AssertionFailed(#condition, {__VA_ARGS__}), false))
#else
#define CARPLAY_ASSERT(condition, ...) static_cast<void>(sizeof(static_cast<bool>(condition)))
#endif