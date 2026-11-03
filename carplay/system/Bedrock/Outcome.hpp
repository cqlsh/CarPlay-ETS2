#pragma once

#include "system/Bedrock/Assert.hpp"
#include "system/Bedrock/Status.hpp"

#include <memory>
#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock {

/** @brief Accepts payload types that an Outcome can hold and move without throwing. */
template <typename Value>
concept OutcomeValue = std::is_object_v<Value> &&
    !std::is_array_v<Value> &&
    std::is_same_v<Value, std::remove_cvref_t<Value>> &&
    std::is_nothrow_move_constructible_v<Value> &&
    std::is_nothrow_destructible_v<Value>;

/**
 * @brief Holds either a value or the fault that prevented it, built through Success or Failure.
 * @details Trivially copyable whenever Value is, so small outcomes travel in registers.
 *          Moving keeps the source state with a moved-from value. Operations without a value
 *          return their fault enumeration directly. Not synchronized.
 */
template <OutcomeValue Value, FaultEnum Fault>
class [[nodiscard]] Outcome {
 public:
  using ValueType = Value;
  using FaultType = Fault;

  /** @brief Builds a successful outcome by constructing the value in place from args. */
  template <typename... Args>
  requires std::is_nothrow_constructible_v<Value, Args...>
  [[nodiscard]] static Outcome Success(Args&&... args) noexcept {
    return Outcome(ValueTag{}, std::forward<Args>(args)...);
  }

  /** @brief Builds a failed outcome from a fault other than None. */
  [[nodiscard]] static Outcome Failure(Fault fault) noexcept {
    CARPLAY_ASSERT(IsFailure(fault), "Failure needs a fault other than None");
    return Outcome(fault);
  }

  Outcome(const Outcome&)
  requires std::is_trivially_copyable_v<Value>
  = default;

  Outcome(const Outcome& other) noexcept
  requires (!std::is_trivially_copyable_v<Value> && std::is_nothrow_copy_constructible_v<Value>)
      : m_fault(other.m_fault) {
    if (other.HasValue()) {
      std::construct_at(std::addressof(m_value), other.m_value);
    }
  }

  Outcome(Outcome&&)
  requires std::is_trivially_copyable_v<Value>
  = default;

  Outcome(Outcome&& other) noexcept
  requires (!std::is_trivially_copyable_v<Value>)
      : m_fault(other.m_fault) {
    if (other.HasValue()) {
      std::construct_at(std::addressof(m_value), std::move(other.m_value));
    }
  }

  Outcome& operator=(const Outcome&)
  requires std::is_trivially_copyable_v<Value>
  = default;

  Outcome& operator=(const Outcome& other) noexcept
  requires (!std::is_trivially_copyable_v<Value> && std::is_nothrow_copy_constructible_v<Value>) {
    if (this != &other) {
      DestroyValue();
      m_fault = other.m_fault;
      if (other.HasValue()) {
        std::construct_at(std::addressof(m_value), other.m_value);
      }
    }
    return *this;
  }

  Outcome& operator=(Outcome&&)
  requires std::is_trivially_copyable_v<Value>
  = default;

  Outcome& operator=(Outcome&& other) noexcept
  requires (!std::is_trivially_copyable_v<Value>) {
    if (this != &other) {
      DestroyValue();
      m_fault = other.m_fault;
      if (other.HasValue()) {
        std::construct_at(std::addressof(m_value), std::move(other.m_value));
      }
    }
    return *this;
  }

  ~Outcome()
  requires std::is_trivially_destructible_v<Value>
  = default;

  ~Outcome() noexcept
  requires (!std::is_trivially_destructible_v<Value>) {
    DestroyValue();
  }

  /** @brief Checks whether a value is present. */
  [[nodiscard]] bool HasValue() const noexcept { return IsSuccess(m_fault); }

  /** @brief Checks whether a fault is present. */
  [[nodiscard]] bool HasFault() const noexcept { return IsFailure(m_fault); }

  /** @brief Converts to true when a value is present. */
  [[nodiscard]] explicit operator bool() const noexcept { return HasValue(); }

  /** @brief Returns the fault, or None when a value is present. */
  [[nodiscard]] Fault GetFault() const noexcept { return m_fault; }

  /** @brief Returns the borrowed value, or nullptr when a fault is present. */
  [[nodiscard]] Value* TryGetValue() noexcept { return HasValue() ? std::addressof(m_value) : nullptr; }
  [[nodiscard]] const Value* TryGetValue() const noexcept { return HasValue() ? std::addressof(m_value) : nullptr; }

  /** @brief Returns the value; calling this on a failed outcome is a bug. */
  [[nodiscard]] Value& GetValue() & noexcept {
    CARPLAY_ASSERT(HasValue(), "GetValue on a failed outcome");
    return m_value;
  }
  [[nodiscard]] const Value& GetValue() const& noexcept {
    CARPLAY_ASSERT(HasValue(), "GetValue on a failed outcome");
    return m_value;
  }
  [[nodiscard]] Value&& GetValue() && noexcept {
    CARPLAY_ASSERT(HasValue(), "GetValue on a failed outcome");
    return std::move(m_value);
  }

 private:
  struct ValueTag {};

  explicit Outcome(Fault fault) noexcept
      : m_fault(fault) {}

  template <typename... Args>
  explicit Outcome(ValueTag, Args&&... args) noexcept
      : m_fault(Fault::None),
        m_value(std::forward<Args>(args)...) {}

  void DestroyValue() noexcept {
    if constexpr (!std::is_trivially_destructible_v<Value>) {
      if (HasValue()) {
        m_value.~Value();
      }
    }
  }

  Fault m_fault;
  union {
    Value m_value;
  };
};

}  // namespace CarPlay::System::Bedrock