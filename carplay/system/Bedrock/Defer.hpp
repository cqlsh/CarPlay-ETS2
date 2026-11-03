#pragma once

#include "system/Bedrock/Target/Toolchain.hpp"

#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock {

/** @brief Accepts callables that Defer can store and call without throwing. */
template <typename Action>
concept DeferAction = std::is_same_v<Action, std::remove_cvref_t<Action>> &&
    std::is_nothrow_move_constructible_v<Action> &&
    std::is_nothrow_destructible_v<Action> &&
    std::is_nothrow_invocable_v<Action&>;

/**
 * @brief Runs a stored action when the scope ends, unless released first.
 * @details Owns the action by value; anything it captures by reference must outlive the guard.
 *          Moving transfers the action and disarms the source. Not synchronized.
 */
template <DeferAction Action>
class [[nodiscard]] Defer {
 public:
  /** @brief Stores the action; its construction must not throw. */
  template <typename Callable>
  requires std::is_same_v<std::remove_cvref_t<Callable>, Action> && std::is_nothrow_constructible_v<Action, Callable&&>
  explicit Defer(Callable&& action) noexcept
      : m_action(std::forward<Callable>(action)) {}

  Defer(const Defer&) = delete;
  Defer& operator=(const Defer&) = delete;
  Defer& operator=(Defer&&) = delete;

  /** @brief Takes over the action and disarms other. */
  Defer(Defer&& other) noexcept
      : m_action(std::move(other.m_action)),
        m_armed(std::exchange(other.m_armed, false)) {}

  ~Defer() noexcept {
    if (m_armed) {
      m_action();
    }
  }

  /** @brief Disarms the guard; the action is destroyed with the guard but never runs. */
  void Release() noexcept { m_armed = false; }

  /** @brief Checks whether the action still runs at scope end. */
  [[nodiscard]] bool IsArmed() const noexcept { return m_armed; }

 private:
  Action m_action;
  bool m_armed = true;
};

template <typename Callable>
Defer(Callable&&) -> Defer<std::remove_cvref_t<Callable>>;

}  // namespace CarPlay::System::Bedrock