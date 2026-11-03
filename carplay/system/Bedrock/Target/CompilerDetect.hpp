#pragma once

// Clang also defines _MSC_VER or __GNUC__ for compatibility, so it has to be tested first.
#if defined(__clang__)
#define CARPLAY_COMPILER_MSVC 0
#define CARPLAY_COMPILER_CLANG 1
#define CARPLAY_COMPILER_GCC 0
#elif defined(_MSC_VER)
#define CARPLAY_COMPILER_MSVC 1
#define CARPLAY_COMPILER_CLANG 0
#define CARPLAY_COMPILER_GCC 0
#elif defined(__GNUC__)
#define CARPLAY_COMPILER_MSVC 0
#define CARPLAY_COMPILER_CLANG 0
#define CARPLAY_COMPILER_GCC 1
#else
#error "CarPlay requires MSVC, Clang or GCC."
#endif

// clang-cl defines _MSC_VER and accepts the MSVC extensions, plain Clang on Windows does not.
#if defined(_MSC_VER)
#define CARPLAY_COMPILER_MSVC_COMPATIBLE 1
#else
#define CARPLAY_COMPILER_MSVC_COMPATIBLE 0
#endif