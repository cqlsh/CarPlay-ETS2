#pragma once

#include "system/Bedrock/Assert.hpp"
#include "system/Bedrock/Storage/Rounding.hpp"

#include <cstddef>
#include <new>
#include <utility>

namespace CarPlay::System::Bedrock::Storage {

/**
 * @brief Owns a block of uninitialized bytes from the heap that starts on a chosen alignment boundary.
 * @details Move-only. Acquiring never throws, and a failed acquire leaves the block as it was.
 *          Meant for large, long-lived buffers such as arena backing, decoder output and staging memory.
 */
class HeapBlock {
 public:
  /** @brief Boundary used when the caller names none; covers every scalar type, AVX2 lanes and a cache line. */
  static constexpr std::size_t DefaultBoundary = 64;

  constexpr HeapBlock() noexcept = default;

  HeapBlock(const HeapBlock&) = delete;
  HeapBlock& operator=(const HeapBlock&) = delete;

  /** @brief Takes over the bytes of other, which ends up empty. */
  HeapBlock(HeapBlock&& other) noexcept
      : m_base(std::exchange(other.m_base, nullptr)),
        m_size(std::exchange(other.m_size, 0)),
        m_boundary(std::exchange(other.m_boundary, 0)) {}

  /** @brief Frees the own bytes and takes over the bytes of other, which ends up empty. */
  HeapBlock& operator=(HeapBlock&& other) noexcept {
    if (this != &other) {
      Free();
      m_base = std::exchange(other.m_base, nullptr);
      m_size = std::exchange(other.m_size, 0);
      m_boundary = std::exchange(other.m_boundary, 0);
    }
    return *this;
  }

  ~HeapBlock() noexcept { Free(); }

  /**
   * @brief Acquires size bytes that start on boundary, a power of two, replacing the current bytes.
   * @return False when the heap has no room; the block then keeps its current bytes. Size zero frees and succeeds.
   */
  [[nodiscard]] bool TryAcquire(std::size_t size, std::size_t boundary = DefaultBoundary) noexcept {
    CARPLAY_ASSERT(IsPowerOfTwo(boundary), "boundary must be a power of two");
    if (size == 0) {
      Free();
      return true;
    }
    void* const base = ::operator new(size, std::align_val_t{boundary}, std::nothrow);
    if (base == nullptr) {
      return false;
    }
    Free();
    m_base = static_cast<std::byte*>(base);
    m_size = size;
    m_boundary = boundary;
    return true;
  }

  /** @brief Returns the bytes to the heap; the block is empty afterwards. */
  void Free() noexcept {
    if (m_base != nullptr) {
      ::operator delete(m_base, std::align_val_t{m_boundary});
      m_base = nullptr;
      m_size = 0;
      m_boundary = 0;
    }
  }

  /** @brief Returns the first byte, or nullptr when the block is empty. */
  [[nodiscard]] std::byte* GetBase() noexcept { return m_base; }

  /** @brief Returns the first byte, or nullptr when the block is empty. */
  [[nodiscard]] const std::byte* GetBase() const noexcept { return m_base; }

  /** @brief Returns the number of bytes, zero when the block is empty. */
  [[nodiscard]] std::size_t GetSize() const noexcept { return m_size; }

  /** @brief Returns the boundary the bytes start on, zero when the block is empty. */
  [[nodiscard]] std::size_t GetBoundary() const noexcept { return m_boundary; }

  /** @brief Checks whether the block holds bytes. */
  [[nodiscard]] bool HasBytes() const noexcept { return m_base != nullptr; }

 private:
  std::byte* m_base = nullptr;
  std::size_t m_size = 0;
  std::size_t m_boundary = 0;
};

}  // namespace CarPlay::System::Bedrock::Storage