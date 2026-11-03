#pragma once

#include "system/Bedrock/Target/SystemDetect.hpp"

#if CARPLAY_OS_WINDOWS
#define CARPLAY_EXPORT __declspec(dllexport)
#else
#define CARPLAY_EXPORT __attribute__((visibility("default")))
#endif