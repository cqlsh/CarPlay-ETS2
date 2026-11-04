#pragma once

#include "system/Bedrock/Assert.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock::Collections {

/** @brief Accepts element types that CircularQueue can hold: objects that move and destroy without throwing. */
template <typename Element>
concept QueueElement = std::is_object_v<Element> && !std::is_const_v<Element> && !std::is_array_v<Element> &&
    std::is_nothrow_move_constructible_v<Element> && std::is_nothrow_destructible_v<Element>;

/**
 * @brief First-in, first-out ring of up to Capacity elements in place, without the heap.
 * @details Enqueue fails when the ring is full; callers that want the newest values to win remove the front first.
 *          Index zero is the oldest element. A moved-from queue is empty. Not synchronized.
 */
template <QueueElement Element, std::size_t Capacity>
requires (Capacity > 0)
class CircularQueue {
 public:
  using ElementType = Element;

  constexpr CircularQueue() noexcept
      : m_unused() {}

  /** @brief Copies the elements of other in queue order. */
  CircularQueue(const CircularQueue& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    AppendCopies(other);
  }

  /** @brief Moves the elements out of other in queue order; other ends up empty. */
  CircularQueue(CircularQueue&& other) noexcept { TakeOver(other); }

  /** @brief Drops the own elements and copies the elements of other in queue order. */
  CircularQueue& operator=(const CircularQueue& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    if (this != &other) {
      Clear();
      AppendCopies(other);
    }
    return *this;
  }

  /** @brief Drops the own elements and moves the elements out of other, which ends up empty. */
  CircularQueue& operator=(CircularQueue&& other) noexcept {
    if (this != &other) {
      Clear();
      TakeOver(other);
    }
    return *this;
  }

  ~CircularQueue() noexcept { Clear(); }

  /** @brief Enqueues a copy at the back; returns false when the ring is full. */
  [[nodiscard]] bool TryEnqueue(const Element& element) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    return TryEmplace(element) != nullptr;
  }

  /** @brief Enqueues by moving at the back; returns false when the ring is full. */
  [[nodiscard]] bool TryEnqueue(Element&& element) noexcept {
    return TryEmplace(std::move(element)) != nullptr;
  }

  /** @brief Builds a new back element in place; returns nullptr when the ring is full. */
  template <typename... Args>
  requires std::is_nothrow_constructible_v<Element, Args...>
  [[nodiscard]] Element* TryEmplace(Args&&... args) noexcept {
    if (m_count == Capacity) {
      return nullptr;
    }
    Element* const element = std::construct_at(m_elements + Slot(m_count), std::forward<Args>(args)...);
    ++m_count;
    return element;
  }

  /** @brief Moves the front element into target and removes it; returns false when the ring is empty. */
  [[nodiscard]] bool TryDequeue(Element& target) noexcept
  requires std::is_nothrow_move_assignable_v<Element>
  {
    if (m_count == 0) {
      return false;
    }
    target = std::move(m_elements[m_head]);
    RemoveFront();
    return true;
  }

  /** @brief Removes the front element, which must exist. */
  void RemoveFront() noexcept {
    CARPLAY_ASSERT(m_count > 0, "queue is empty");
    std::destroy_at(m_elements + m_head);
    m_head = Slot(1);
    --m_count;
  }

  /** @brief Destroys every element. */
  void Clear() noexcept {
    while (m_count > 0) {
      RemoveFront();
    }
    m_head = 0;
  }

  /** @brief Returns the fixed number of elements the ring can hold. */
  [[nodiscard]] static constexpr std::size_t GetCapacity() noexcept { return Capacity; }

  /** @brief Returns the number of elements held. */
  [[nodiscard]] std::size_t GetCount() const noexcept { return m_count; }

  /** @brief Checks whether no element is held. */
  [[nodiscard]] bool IsEmpty() const noexcept { return m_count == 0; }

  /** @brief Checks whether the capacity is used up. */
  [[nodiscard]] bool IsFull() const noexcept { return m_count == Capacity; }

  /** @brief Returns the element at index counted from the front, which must be below the count. */
  [[nodiscard]] Element& operator[](std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_elements[Slot(index)];
  }

  /** @brief Returns the element at index counted from the front, which must be below the count. */
  [[nodiscard]] const Element& operator[](std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_elements[Slot(index)];
  }

  /** @brief Returns the oldest element, which must exist. */
  [[nodiscard]] Element& GetFront() noexcept { return (*this)[0]; }

  /** @brief Returns the oldest element, which must exist. */
  [[nodiscard]] const Element& GetFront() const noexcept { return (*this)[0]; }

  /** @brief Returns the newest element, which must exist. */
  [[nodiscard]] Element& GetBack() noexcept {
    CARPLAY_ASSERT(m_count > 0, "queue is empty");
    return m_elements[Slot(m_count - 1)];
  }

  /** @brief Returns the newest element, which must exist. */
  [[nodiscard]] const Element& GetBack() const noexcept {
    CARPLAY_ASSERT(m_count > 0, "queue is empty");
    return m_elements[Slot(m_count - 1)];
  }

 private:
  [[nodiscard]] std::size_t Slot(std::size_t offset) const noexcept {
    const std::size_t slot = m_head + offset;
    return slot < Capacity ? slot : slot - Capacity;
  }

  void AppendCopies(const CircularQueue& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_elements + i, other[i]);
    }
    m_count = other.m_count;
  }

  void TakeOver(CircularQueue& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_elements + i, std::move(other[i]));
    }
    m_count = other.m_count;
    other.Clear();
  }

  // Gives the union an active member, so the constexpr default constructor is a constant initializer.
  union {
    char m_unused;
    Element m_elements[Capacity];
  };
  std::size_t m_head = 0;
  std::size_t m_count = 0;
};

}  // namespace CarPlay::System::Bedrock::Collections