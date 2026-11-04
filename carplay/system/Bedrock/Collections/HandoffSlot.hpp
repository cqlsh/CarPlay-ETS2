#pragma once

#include "system/Bedrock/Target/Toolchain.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#if CARPLAY_COMPILER_MSVC_COMPATIBLE
// MSVC reports the padding that the cache-line alignment asks for; here that padding is the point.
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

namespace CarPlay::System::Bedrock::Collections {

/** @brief Accepts value types a HandoffSlot can pass: default constructible, move assignable and destructible without throwing. */
template <typename Value>
concept HandoffValue = std::is_object_v<Value> && !std::is_const_v<Value> && !std::is_array_v<Value> &&
    std::is_nothrow_default_constructible_v<Value> && std::is_nothrow_move_assignable_v<Value> && std::is_nothrow_destructible_v<Value>;

/**
 * @brief Passes the newest value from one producer thread to one consumer thread without locks or waiting.
 * @details Three cells rotate through one atomic word: the producer fills its draft and publishes it, the consumer
 *          receives the latest published cell and reads it until the next receive. Older values are dropped, not queued.
 *          Exactly one thread publishes and exactly one receives; each side lives on its own cache line.
 */
template <HandoffValue Value>
class HandoffSlot {
 public:
  constexpr HandoffSlot() noexcept = default;

  HandoffSlot(const HandoffSlot&) = delete;
  HandoffSlot& operator=(const HandoffSlot&) = delete;
  HandoffSlot(HandoffSlot&&) = delete;
  HandoffSlot& operator=(HandoffSlot&&) = delete;

  /** @brief Returns the cell the producer fills next; it still holds what was published two rounds ago. */
  [[nodiscard]] Value& GetDraft() noexcept { return m_cells[m_draft].value; }

  /** @brief Publishes the draft; the producer continues in another cell. */
  void Publish() noexcept {
    const std::uint32_t previous = m_state.exchange(m_draft | FreshFlag, std::memory_order_acq_rel);
    m_draft = previous & IndexMask;
  }

  /** @brief Copies value into the draft and publishes it. */
  void Publish(const Value& value) noexcept
  requires std::is_nothrow_copy_assignable_v<Value>
  {
    GetDraft() = value;
    Publish();
  }

  /** @brief Moves value into the draft and publishes it. */
  void Publish(Value&& value) noexcept {
    GetDraft() = std::move(value);
    Publish();
  }

  /** @brief Checks whether a value was published since the last receive. */
  [[nodiscard]] bool HasFresh() const noexcept {
    return (m_state.load(std::memory_order_relaxed) & FreshFlag) != 0;
  }

  /** @brief Takes the newest published value when there is one; returns false when nothing new arrived. */
  [[nodiscard]] bool TryReceive() noexcept {
    if (!HasFresh()) {
      return false;
    }
    const std::uint32_t previous = m_state.exchange(m_received, std::memory_order_acq_rel);
    m_received = previous & IndexMask;
    return true;
  }

  /** @brief Returns the value received last; it stays put until the next receive. */
  [[nodiscard]] Value& GetReceived() noexcept { return m_cells[m_received].value; }

  /** @brief Returns the value received last; it stays put until the next receive. */
  [[nodiscard]] const Value& GetReceived() const noexcept { return m_cells[m_received].value; }

 private:
  static constexpr std::uint32_t IndexMask = 3;
  static constexpr std::uint32_t FreshFlag = 4;
  static constexpr std::size_t CacheLine = 64;
  static constexpr std::size_t CellAlignment = alignof(Value) > CacheLine ? alignof(Value) : CacheLine;

  struct alignas(CellAlignment) Cell {
    Value value{};
  };

  alignas(CacheLine) std::atomic<std::uint32_t> m_state{1};
  alignas(CacheLine) std::uint32_t m_draft = 0;
  alignas(CacheLine) std::uint32_t m_received = 2;
  Cell m_cells[3]{};
};

}  // namespace CarPlay::System::Bedrock::Collections

#if CARPLAY_COMPILER_MSVC_COMPATIBLE
#pragma warning(pop)
#endif