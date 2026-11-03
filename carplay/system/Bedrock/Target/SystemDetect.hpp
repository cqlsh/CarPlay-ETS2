#pragma once

#if defined(_WIN32)
#define CARPLAY_OS_WINDOWS 1
#define CARPLAY_OS_LINUX 0
#define CARPLAY_OS_MACOS 0
#elif defined(__linux__) && !defined(__ANDROID__)
#define CARPLAY_OS_WINDOWS 0
#define CARPLAY_OS_LINUX 1
#define CARPLAY_OS_MACOS 0
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#error "CarPlay supports macOS but no other Apple platform."
#endif

#define CARPLAY_OS_WINDOWS 0
#define CARPLAY_OS_LINUX 0
#define CARPLAY_OS_MACOS 1
#else
#error "CarPlay requires Windows, Linux or macOS."
#endif