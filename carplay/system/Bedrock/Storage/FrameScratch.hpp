#pragma once

#include "system/Bedrock/Assert.hpp"
#include "system/Bedrock/Storage/HeapBlock.hpp"
#include "system/Bedrock/Storage/Rounding.hpp"

#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock::Storage {

/** @brief Accepts element types that may live in uninitialized scratch bytes: trivial to create, copy and drop. */
template <typename Element>
concept ScratchElement = std::is_object_v<Element> && !std::is_const_v<Element> &&
    std::is_trivially_default_constructible_v<Element> && std::is_trivially_copyable_v<Element>;

/**
 * @brief Hands out bytes from one heap block by moving a cursor forward; Clear takes everything back at once.
 * @details Sized once for the worst frame, then free of heap traffic: a take is a padding computation and an add.
 *          Pointers stay valid until the next Clear, a Rewind below them or a TryReserve. Not synchronized.
 */
class FrameScratch {
 public:
  /** @brief Boundary used when a take names none; enough for every scalar type and SSE lanes. */
  static constexpr std::size_t DefaultBoundary = 16;

  constexpr FrameScratch() noexcept = default;

  FrameScratch(const FrameScratch&) = delete;
  FrameScratch& operator=(const FrameScratch&) = delete;

  /** @brief Takes over the backing and cursor of other, which ends up without backing. */
  FrameScratch(FrameScratch&& other) noexcept
      : m_backing(std::move(other.m_backing)),
        m_cursor(std::exchange(other.m_cursor, 0)),
        m_highWater(std::exchange(other.m_highWater, 0)) {}

  /** @brief Frees the own backing and takes over the backing and cursor of other, which ends up without backing. */
  FrameScratch& operator=(FrameScratch&& other) noexcept {
    if (this != &other) {
      m_backing = std::move(other.m_backing);
      m_cursor = std::exchange(other.m_cursor, 0);
      m_highWater = std::exchange(other.m_highWater, 0);
    }
    return *this;
  }

  ~FrameScratch() noexcept = default;

  /**
   * @brief Acquires capacity bytes of backing from the heap and starts empty; zero frees the backing.
   * @return False when the heap has no room; the old backing and cursor then stay as they were.
   */
  [[nodiscard]] bool TryReserve(std::size_t capacity) noexcept {
    if (!m_backing.TryAcquire(capacity)) {
      return false;
    }
    m_cursor = 0;
    m_highWater = 0;
    return true;
  }

  /**
   * @brief Hands out size uninitialized bytes that start on boundary, a power of two.
   * @return nullptr when the backing has no room left; size zero still returns a usable address.
   */
  [[nodiscard]] std::byte* TryTake(std::size_t size, std::size_t boundary = DefaultBoundary) noexcept {
    CARPLAY_ASSERT(IsPowerOfTwo(boundary), "boundary must be a power of two");
    std::byte* const base = m_backing.GetBase();
    if (base == nullptr) {
      return nullptr;
    }
    const std::size_t padding = PaddingToBoundary(AddressValue(base + m_cursor), boundary);
    const std::size_t remaining = m_backing.GetSize() - m_cursor;
    if (padding > remaining || size > remaining - padding) {
      return nullptr;
    }
    std::byte* const bytes = base + m_cursor + padding;
    m_cursor += padding + size;
    if (m_cursor > m_highWater) {
      m_highWater = m_cursor;
    }
    return bytes;
  }

  /**
   * @brief Hands out uninitialized storage for count elements, on the element alignment unless a wider boundary is named.
   * @return nullptr when the backing has no room left or the byte count overflows.
   */
  template <ScratchElement Element>
  [[nodiscard]] Element* TryTakeArray(std::size_t count, std::size_t boundary = alignof(Element)) noexcept {
    CARPLAY_ASSERT(boundary >= alignof(Element), "boundary below the element alignment");
    if (count > (std::numeric_limits<std::size_t>::max)() / sizeof(Element)) {
      return nullptr;
    }
    return reinterpret_cast<Element*>(TryTake(count * sizeof(Element), boundary));
  }

  /** @brief Returns the cursor, so Rewind can come back to it. */
  [[nodiscard]] std::size_t GetCursor() const noexcept { return m_cursor; }

  /** @brief Moves the cursor back to a value from GetCursor; bytes handed out after that point are free again. */
  void Rewind(std::size_t cursor) noexcept {
    CARPLAY_ASSERT(cursor <= m_cursor, "rewind past the cursor");
    if (cursor < m_cursor) {
      m_cursor = cursor;
    }
  }

  /** @brief Takes every byte back; the backing and the high-water mark stay. */
  void Clear() noexcept { m_cursor = 0; }

  /** @brief Returns the size of the backing, zero without one. */
  [[nodiscard]] std::size_t GetCapacity() const noexcept { return m_backing.GetSize(); }

  /** @brief Returns the bytes handed out since the last Clear, padding included. */
  [[nodiscard]] std::size_t GetUsed() const noexcept { return m_cursor; }

  /** @brief Returns the bytes still available before padding. */
  [[nodiscard]] std::size_t GetRemaining() const noexcept { return m_backing.GetSize() - m_cursor; }

  /** @brief Returns the most bytes in use at once since the backing was reserved; sizes the next build. */
  [[nodiscard]] std::size_t GetHighWater() const noexcept { return m_highWater; }

  /** @brief Checks whether a backing is reserved. */
  [[nodiscard]] bool HasBacking() const noexcept { return m_backing.HasBytes(); }

 private:
  HeapBlock m_backing;
  std::size_t m_cursor = 0;
  std::size_t m_highWater = 0;
};

}  // namespace CarPlay::System::Bedrock::Storage