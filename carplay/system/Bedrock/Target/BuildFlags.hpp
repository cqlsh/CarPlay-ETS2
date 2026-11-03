#pragma once

// Outside the CMake build the debug flag follows NDEBUG, like the standard assert.
#ifndef CARPLAY_DEBUG_BUILD
#if defined(NDEBUG)
#define CARPLAY_DEBUG_BUILD 0
#else
#define CARPLAY_DEBUG_BUILD 1
#endif
#endif

#ifndef CARPLAY_SKIN_IOS
#define CARPLAY_SKIN_IOS 1
#endif

#ifndef CARPLAY_SKIN_ANDROID
#define CARPLAY_SKIN_ANDROID 0
#endif

#if defined(__AVX2__)
#define CARPLAY_SIMD_AVX2 1
#else
#define CARPLAY_SIMD_AVX2 0
#endif

static_assert(CARPLAY_DEBUG_BUILD == 0 || CARPLAY_DEBUG_BUILD == 1, "CARPLAY_DEBUG_BUILD must be 0 or 1.");
static_assert(CARPLAY_SKIN_IOS == 0 || CARPLAY_SKIN_IOS == 1, "CARPLAY_SKIN_IOS must be 0 or 1.");
static_assert(CARPLAY_SKIN_ANDROID == 0 || CARPLAY_SKIN_ANDROID == 1, "CARPLAY_SKIN_ANDROID must be 0 or 1.");
static_assert(CARPLAY_SKIN_IOS == 1 || CARPLAY_SKIN_ANDROID == 1, "At least one skin must be compiled in.");