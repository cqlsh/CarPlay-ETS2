#pragma once

#include "system/Bedrock/Assert.hpp"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock::Collections {

/** @brief Accepts element types that BoundedList can hold: objects that move and destroy without throwing. */
template <typename Element>
concept ListElement = std::is_object_v<Element> && !std::is_const_v<Element> && !std::is_array_v<Element> &&
    std::is_nothrow_move_constructible_v<Element> && std::is_nothrow_destructible_v<Element>;

/**
 * @brief Holds up to Capacity elements in place, without the heap; appends fail instead of growing.
 * @details Elements keep their insertion order and are reachable by index or as a pointer range.
 *          A moved-from list is empty. Not synchronized.
 */
template <ListElement Element, std::size_t Capacity>
requires (Capacity > 0)
class BoundedList {
 public:
  using ElementType = Element;

  constexpr BoundedList() noexcept
      : m_unused() {}

  /** @brief Copies the elements of other. */
  BoundedList(const BoundedList& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    AppendCopies(other);
  }

  /** @brief Moves the elements out of other, which ends up empty. */
  BoundedList(BoundedList&& other) noexcept { TakeOver(other); }

  /** @brief Drops the own elements and copies the elements of other. */
  BoundedList& operator=(const BoundedList& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    if (this != &other) {
      Clear();
      AppendCopies(other);
    }
    return *this;
  }

  /** @brief Drops the own elements and moves the elements out of other, which ends up empty. */
  BoundedList& operator=(BoundedList&& other) noexcept {
    if (this != &other) {
      Clear();
      TakeOver(other);
    }
    return *this;
  }

  ~BoundedList() noexcept { Clear(); }

  /** @brief Appends a copy; returns false when the list is full. */
  [[nodiscard]] bool TryAppend(const Element& element) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    return TryEmplace(element) != nullptr;
  }

  /** @brief Appends by moving; returns false when the list is full. */
  [[nodiscard]] bool TryAppend(Element&& element) noexcept {
    return TryEmplace(std::move(element)) != nullptr;
  }

  /** @brief Builds a new last element in place; returns nullptr when the list is full. */
  template <typename... Args>
  requires std::is_nothrow_constructible_v<Element, Args...>
  [[nodiscard]] Element* TryEmplace(Args&&... args) noexcept {
    if (m_count == Capacity) {
      return nullptr;
    }
    Element* const element = std::construct_at(m_elements + m_count, std::forward<Args>(args)...);
    ++m_count;
    return element;
  }

  /** @brief Inserts a copy at index, at most the count, and shifts the rest up; returns false when the list is full. */
  [[nodiscard]] bool TryInsertAt(std::size_t index, const Element& element) noexcept
  requires std::is_nothrow_copy_constructible_v<Element>
  {
    return TryPlaceAt(index, element);
  }

  /** @brief Inserts by moving at index, at most the count, and shifts the rest up; returns false when the list is full. */
  [[nodiscard]] bool TryInsertAt(std::size_t index, Element&& element) noexcept {
    return TryPlaceAt(index, std::move(element));
  }

  /** @brief Removes the element at index and shifts the rest down, so the order stays. */
  void RemoveAt(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    for (std::size_t i = index + 1; i < m_count; ++i) {
      Replace(i - 1, std::move(m_elements[i]));
    }
    RemoveLast();
  }

  /** @brief Removes the element at index by moving the last one into its place; the order changes. */
  void RemoveAtUnordered(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    if (index + 1 < m_count) {
      Replace(index, std::move(m_elements[m_count - 1]));
    }
    RemoveLast();
  }

  /** @brief Removes the last element, which must exist. */
  void RemoveLast() noexcept {
    CARPLAY_ASSERT(m_count > 0, "list is empty");
    --m_count;
    std::destroy_at(m_elements + m_count);
  }

  /** @brief Destroys every element. */
  void Clear() noexcept {
    while (m_count > 0) {
      RemoveLast();
    }
  }

  /** @brief Returns the fixed number of elements the list can hold. */
  [[nodiscard]] static constexpr std::size_t GetCapacity() noexcept { return Capacity; }

  /** @brief Returns the number of elements held. */
  [[nodiscard]] std::size_t GetCount() const noexcept { return m_count; }

  /** @brief Checks whether no element is held. */
  [[nodiscard]] bool IsEmpty() const noexcept { return m_count == 0; }

  /** @brief Checks whether the capacity is used up. */
  [[nodiscard]] bool IsFull() const noexcept { return m_count == Capacity; }

  /** @brief Returns the element at index, which must be below the count. */
  [[nodiscard]] Element& operator[](std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_elements[index];
  }

  /** @brief Returns the element at index, which must be below the count. */
  [[nodiscard]] const Element& operator[](std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_elements[index];
  }

  /** @brief Returns the first element, which must exist. */
  [[nodiscard]] Element& GetFirst() noexcept { return (*this)[0]; }

  /** @brief Returns the first element, which must exist. */
  [[nodiscard]] const Element& GetFirst() const noexcept { return (*this)[0]; }

  /** @brief Returns the last element, which must exist. */
  [[nodiscard]] Element& GetLast() noexcept {
    CARPLAY_ASSERT(m_count > 0, "list is empty");
    return m_elements[m_count - 1];
  }

  /** @brief Returns the last element, which must exist. */
  [[nodiscard]] const Element& GetLast() const noexcept {
    CARPLAY_ASSERT(m_count > 0, "list is empty");
    return m_elements[m_count - 1];
  }

  [[nodiscard]] Element* begin() noexcept { return m_elements; }
  [[nodiscard]] const Element* begin() const noexcept { return m_elements; }
  [[nodiscard]] Element* end() noexcept { return m_elements + m_count; }
  [[nodiscard]] const Element* end() const noexcept { return m_elements + m_count; }

 private:
  template <typename Source>
  bool TryPlaceAt(std::size_t index, Source&& source) noexcept {
    CARPLAY_ASSERT(index <= m_count, "index past the count");
    if (m_count == Capacity) {
      return false;
    }
    if (index == m_count) {
      return TryEmplace(std::forward<Source>(source)) != nullptr;
    }
    std::construct_at(m_elements + m_count, std::move(m_elements[m_count - 1]));
    ++m_count;
    for (std::size_t i = m_count - 2; i > index; --i) {
      Replace(i, std::move(m_elements[i - 1]));
    }
    Replace(index, std::forward<Source>(source));
    return true;
  }

  template <typename Source>
  void Replace(std::size_t index, Source&& source) noexcept {
    std::destroy_at(m_elements + index);
    std::construct_at(m_elements + index, std::forward<Source>(source));
  }

  void AppendCopies(const BoundedList& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_elements + i, other.m_elements[i]);
    }
    m_count = other.m_count;
  }

  void TakeOver(BoundedList& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_elements + i, std::move(other.m_elements[i]));
    }
    m_count = other.m_count;
    other.Clear();
  }

  // Gives the union an active member, so the constexpr default constructor is a constant initializer.
  union {
    char m_unused;
    Element m_elements[Capacity];
  };
  std::size_t m_count = 0;
};

}  // namespace CarPlay::System::Bedrock::Collections