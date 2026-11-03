#include "system/Bedrock/Assert.hpp"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#if CARPLAY_OS_WINDOWS
#include <Windows.h>
#endif

namespace CarPlay::System::Bedrock {

namespace {

constinit std::atomic<AssertionSink> g_assertionSink{nullptr};

}  // namespace

void SetAssertionSink(AssertionSink sink) noexcept {
  g_assertionSink.store(sink, std::memory_order_relaxed);
}

AssertionSink GetAssertionSink() noexcept {
  return g_assertionSink.load(std::memory_order_relaxed);
}

void AssertionFailed(const char* expression, const char* message, std::source_location location) noexcept {
  if (const AssertionSink sink = g_assertionSink.load(std::memory_order_relaxed); sink != nullptr) {
    sink(expression, message, location);
  }
  char report[2048];
  const int length = std::snprintf(report, sizeof(report), "CarPlay assertion failed: %s\n  at %s:%u in %s\n", expression, location.file_name(), static_cast<unsigned>(location.line()), location.function_name());
  if (message != nullptr && length > 0 && static_cast<std::size_t>(length) < sizeof(report)) {
    std::snprintf(report + length, sizeof(report) - static_cast<std::size_t>(length), "  %s\n", message);
  }
  std::fputs(report, stderr);
  std::fflush(stderr);
#if CARPLAY_OS_WINDOWS
  ::OutputDebugStringA(report);
  if (::IsDebuggerPresent() != 0) {
    ::DebugBreak();
  }
  // The debug CRT would otherwise block the game behind a message box hidden by the fullscreen window.
  _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
  std::abort();
}

}  // namespace CarPlay::System::Bedrock