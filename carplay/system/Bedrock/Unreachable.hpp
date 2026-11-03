#pragma once

#include "system/Bedrock/Assert.hpp"

/**
 * @brief Marks a code path that cannot be reached, with an optional const char* message.
 * @details Debug builds report through AssertionFailed; release builds tell the optimizer the path is dead.
 *          Reaching it in a release build is undefined behavior.
 */
#if CARPLAY_DEBUG_BUILD
#define CARPLAY_UNREACHABLE(...) ::CarPlay::System::Bedrock::AssertionFailed("unreachable code", {__VA_ARGS__})
#elif CARPLAY_COMPILER_MSVC_COMPATIBLE
#define CARPLAY_UNREACHABLE(...) __assume(0)
#else
#define CARPLAY_UNREACHABLE(...) __builtin_unreachable()
#endif