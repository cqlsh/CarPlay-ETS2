#pragma once

// MSVC reports the real language level in _MSVC_LANG unless /Zc:__cplusplus is set.
#if defined(_MSVC_LANG)
#define CARPLAY_LANGUAGE_VERSION _MSVC_LANG
#elif defined(__cplusplus)
#define CARPLAY_LANGUAGE_VERSION __cplusplus
#else
#define CARPLAY_LANGUAGE_VERSION 0L
#endif

#if CARPLAY_LANGUAGE_VERSION < 202002L
#error "CarPlay requires C++20 or newer."
#endif