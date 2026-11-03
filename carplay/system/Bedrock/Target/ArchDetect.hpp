#pragma once

// ARM64EC defines the x64 macros as well, but its ABI differs from both x64 and ARM64.
#if defined(_M_ARM64EC) || defined(__arm64ec__)
#error "CarPlay does not support the ARM64EC ABI."
#elif defined(_M_X64) || defined(__x86_64__)
#define CARPLAY_ARCH_X64 1
#define CARPLAY_ARCH_ARM64 0
#elif defined(_M_ARM64) || defined(__aarch64__)
#define CARPLAY_ARCH_X64 0
#define CARPLAY_ARCH_ARM64 1
#else
#error "CarPlay requires an x86-64 or ARM64 target."
#endif

static_assert(sizeof(void*) == 8, "CarPlay requires 64-bit pointers.");