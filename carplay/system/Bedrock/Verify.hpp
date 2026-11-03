#pragma once

#include "system/Bedrock/Assert.hpp"

/**
 * @brief Evaluates condition in every build and checks it like CARPLAY_ASSERT in debug builds.
 * @details For calls that must run in release builds but can only fail because of a bug.
 *          Release builds discard the result, so a failing condition continues silently.
 */
#if CARPLAY_DEBUG_BUILD
#define CARPLAY_VERIFY(condition, ...) CARPLAY_ASSERT(condition, __VA_ARGS__)
#else
#define CARPLAY_VERIFY(condition, ...) static_cast<void>(static_cast<bool>(condition))
#endif