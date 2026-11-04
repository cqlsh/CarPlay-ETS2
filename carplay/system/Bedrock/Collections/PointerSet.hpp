#pragma once

#include "system/Bedrock/Assert.hpp"

#include <cstddef>
#include <type_traits>

namespace CarPlay::System::Bedrock::Collections {

/**
 * @brief Keeps up to Capacity distinct pointers to Pointee in place, without the heap; adds fail instead of growing.
 * @details A pointer counts as present when the same address is stored, and nullptr is never stored. The lookup
 *          scans from the front, which beats hashing for the small registries this is meant for. Removing a pointer
 *          moves the last one into its place, so the order changes. Pointee is an object type or void and may be
 *          incomplete or const. A moved-from set is empty. Not synchronized.
 */
template <typename Pointee, std::size_t Capacity>
requires ((std::is_object_v<Pointee> || std::is_void_v<Pointee>) && Capacity > 0)
class PointerSet {
 public:
  using Pointer = Pointee*;

  constexpr PointerSet() noexcept
      : m_unused() {}

  /** @brief Copies the pointers of other. */
  PointerSet(const PointerSet& other) noexcept { CopyFrom(other); }

  /** @brief Takes the pointers of other, which ends up empty. */
  PointerSet(PointerSet&& other) noexcept {
    CopyFrom(other);
    other.m_count = 0;
  }

  /** @brief Drops the own pointers and copies the pointers of other. */
  PointerSet& operator=(const PointerSet& other) noexcept {
    if (this != &other) {
      CopyFrom(other);
    }
    return *this;
  }

  /** @brief Drops the own pointers and takes the pointers of other, which ends up empty. */
  PointerSet& operator=(PointerSet&& other) noexcept {
    if (this != &other) {
      CopyFrom(other);
      other.m_count = 0;
    }
    return *this;
  }

  /** @brief Adds pointer, which must not be null; returns false when it is present already or the set is full. */
  [[nodiscard]] bool TryAdd(Pointer pointer) noexcept {
    CARPLAY_ASSERT(pointer != nullptr, "null pointer");
    if (m_count == Capacity || FindIndex(pointer) != m_count) {
      return false;
    }
    m_slots[m_count] = pointer;
    ++m_count;
    return true;
  }

  /** @brief Removes pointer by moving the last pointer into its place; returns false when it is absent. */
  bool Remove(const Pointee* pointer) noexcept {
    const std::size_t index = FindIndex(pointer);
    if (index == m_count) {
      return false;
    }
    RemoveAt(index);
    return true;
  }

  /** @brief Removes the pointer at index by moving the last pointer into its place; the order changes. */
  void RemoveAt(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    --m_count;
    m_slots[index] = m_slots[m_count];
  }

  /** @brief Forgets every pointer. */
  void Clear() noexcept { m_count = 0; }

  /** @brief Checks whether pointer is present. */
  [[nodiscard]] bool Has(const Pointee* pointer) const noexcept { return FindIndex(pointer) != m_count; }

  /** @brief Looks up the index of pointer; returns false and leaves index alone when it is absent. */
  [[nodiscard]] bool TryFindIndex(const Pointee* pointer, std::size_t& index) const noexcept {
    const std::size_t found = FindIndex(pointer);
    if (found == m_count) {
      return false;
    }
    index = found;
    return true;
  }

  /** @brief Returns the fixed number of pointers the set can hold. */
  [[nodiscard]] static constexpr std::size_t GetCapacity() noexcept { return Capacity; }

  /** @brief Returns the number of pointers held. */
  [[nodiscard]] std::size_t GetCount() const noexcept { return m_count; }

  /** @brief Checks whether no pointer is held. */
  [[nodiscard]] bool IsEmpty() const noexcept { return m_count == 0; }

  /** @brief Checks whether the capacity is used up. */
  [[nodiscard]] bool IsFull() const noexcept { return m_count == Capacity; }

  /** @brief Returns the pointer at index, which must be below the count. */
  [[nodiscard]] Pointer operator[](std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_slots[index];
  }

  [[nodiscard]] const Pointer* begin() const noexcept { return m_slots; }
  [[nodiscard]] const Pointer* end() const noexcept { return m_slots + m_count; }

 private:
  [[nodiscard]] std::size_t FindIndex(const Pointee* pointer) const noexcept {
    for (std::size_t i = 0; i < m_count; ++i) {
      if (m_slots[i] == pointer) {
        return i;
      }
    }
    return m_count;
  }

  void CopyFrom(const PointerSet& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      m_slots[i] = other.m_slots[i];
    }
    m_count = other.m_count;
  }

  // Gives the union an active member, so the constexpr default constructor is a constant initializer.
  union {
    char m_unused;
    Pointer m_slots[Capacity];
  };
  std::size_t m_count = 0;
};

}  // namespace CarPlay::System::Bedrock::Collections