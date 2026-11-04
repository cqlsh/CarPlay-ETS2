#pragma once

#include "system/Bedrock/Assert.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace CarPlay::System::Bedrock::Collections {

/** @brief Accepts key types a FlatMap can hold: objects that compare with == and move and destroy without throwing. */
template <typename Key>
concept MapKey = std::is_object_v<Key> && !std::is_const_v<Key> && !std::is_array_v<Key> &&
    std::is_nothrow_move_constructible_v<Key> && std::is_nothrow_destructible_v<Key> &&
    requires (const Key& left, const Key& right) {
      { left == right } noexcept -> std::convertible_to<bool>;
    };

/** @brief Accepts value types a FlatMap can hold: objects that move and destroy without throwing. */
template <typename Value>
concept MapValue = std::is_object_v<Value> && !std::is_const_v<Value> && !std::is_array_v<Value> &&
    std::is_nothrow_move_constructible_v<Value> && std::is_nothrow_destructible_v<Value>;

/**
 * @brief Maps up to Capacity keys to values in place, without the heap; inserts fail instead of growing.
 * @details Keys and values sit in two parallel arrays, and a lookup scans the keys from the front, which beats
 *          hashing for the small tables this is meant for. Removing a pair moves the last pair into its place,
 *          so the order changes. A moved-from map is empty. Not synchronized.
 */
template <MapKey Key, MapValue Value, std::size_t Capacity>
requires (Capacity > 0)
class FlatMap {
 public:
  using KeyType = Key;
  using ValueType = Value;

  constexpr FlatMap() noexcept
      : m_unusedKeys(),
        m_unusedValues() {}

  /** @brief Copies the pairs of other. */
  FlatMap(const FlatMap& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_copy_constructible_v<Value>
  {
    AppendCopies(other);
  }

  /** @brief Moves the pairs out of other, which ends up empty. */
  FlatMap(FlatMap&& other) noexcept { TakeOver(other); }

  /** @brief Drops the own pairs and copies the pairs of other. */
  FlatMap& operator=(const FlatMap& other) noexcept
  requires std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_copy_constructible_v<Value>
  {
    if (this != &other) {
      Clear();
      AppendCopies(other);
    }
    return *this;
  }

  /** @brief Drops the own pairs and moves the pairs out of other, which ends up empty. */
  FlatMap& operator=(FlatMap&& other) noexcept {
    if (this != &other) {
      Clear();
      TakeOver(other);
    }
    return *this;
  }

  ~FlatMap() noexcept { Clear(); }

  /** @brief Adds a pair under a key not yet present; returns nullptr when the key exists or the map is full. */
  template <typename ValueArg>
  requires std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<Value, ValueArg>
  [[nodiscard]] Value* TryInsert(const Key& key, ValueArg&& value) noexcept {
    return TryEmplace(key, std::forward<ValueArg>(value));
  }

  /** @brief Adds a pair under a key not yet present; returns nullptr when the key exists or the map is full. */
  template <typename ValueArg>
  requires std::is_nothrow_constructible_v<Value, ValueArg>
  [[nodiscard]] Value* TryInsert(Key&& key, ValueArg&& value) noexcept {
    return TryEmplace(std::move(key), std::forward<ValueArg>(value));
  }

  /** @brief Builds the value in place under a key not yet present; returns nullptr when the key exists or the map is full. */
  template <typename... Args>
  requires std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<Value, Args...>
  [[nodiscard]] Value* TryEmplace(const Key& key, Args&&... args) noexcept {
    if (m_count == Capacity || FindIndex(key) != m_count) {
      return nullptr;
    }
    return &Place(key, std::forward<Args>(args)...);
  }

  /** @brief Builds the value in place under a key not yet present; returns nullptr when the key exists or the map is full. */
  template <typename... Args>
  requires std::is_nothrow_constructible_v<Value, Args...>
  [[nodiscard]] Value* TryEmplace(Key&& key, Args&&... args) noexcept {
    if (m_count == Capacity || FindIndex(key) != m_count) {
      return nullptr;
    }
    return &Place(std::move(key), std::forward<Args>(args)...);
  }

  /** @brief Stores value under key, replacing an existing value; returns false when the key is new and the map is full. */
  template <typename ValueArg>
  requires std::is_nothrow_copy_constructible_v<Key> && std::is_nothrow_constructible_v<Value, ValueArg>
  [[nodiscard]] bool TryAssign(const Key& key, ValueArg&& value) noexcept {
    const std::size_t index = FindIndex(key);
    if (index != m_count) {
      Replace(m_values + index, std::forward<ValueArg>(value));
      return true;
    }
    if (m_count == Capacity) {
      return false;
    }
    Place(key, std::forward<ValueArg>(value));
    return true;
  }

  /** @brief Stores value under key, replacing an existing value; returns false when the key is new and the map is full. */
  template <typename ValueArg>
  requires std::is_nothrow_constructible_v<Value, ValueArg>
  [[nodiscard]] bool TryAssign(Key&& key, ValueArg&& value) noexcept {
    const std::size_t index = FindIndex(key);
    if (index != m_count) {
      Replace(m_values + index, std::forward<ValueArg>(value));
      return true;
    }
    if (m_count == Capacity) {
      return false;
    }
    Place(std::move(key), std::forward<ValueArg>(value));
    return true;
  }

  /** @brief Returns the value stored under key, or nullptr when the key is absent. */
  [[nodiscard]] Value* TryFind(const Key& key) noexcept {
    const std::size_t index = FindIndex(key);
    return index != m_count ? m_values + index : nullptr;
  }

  /** @brief Returns the value stored under key, or nullptr when the key is absent. */
  [[nodiscard]] const Value* TryFind(const Key& key) const noexcept {
    const std::size_t index = FindIndex(key);
    return index != m_count ? m_values + index : nullptr;
  }

  /** @brief Checks whether a value is stored under key. */
  [[nodiscard]] bool Has(const Key& key) const noexcept { return FindIndex(key) != m_count; }

  /** @brief Looks up the index of key for the At accessors; returns false and leaves index alone when the key is absent. */
  [[nodiscard]] bool TryFindIndex(const Key& key, std::size_t& index) const noexcept {
    const std::size_t found = FindIndex(key);
    if (found == m_count) {
      return false;
    }
    index = found;
    return true;
  }

  /** @brief Returns the value stored under key, which must be present. */
  [[nodiscard]] Value& GetValue(const Key& key) noexcept {
    const std::size_t index = FindIndex(key);
    CARPLAY_ASSERT(index != m_count, "key not found");
    return m_values[index];
  }

  /** @brief Returns the value stored under key, which must be present. */
  [[nodiscard]] const Value& GetValue(const Key& key) const noexcept {
    const std::size_t index = FindIndex(key);
    CARPLAY_ASSERT(index != m_count, "key not found");
    return m_values[index];
  }

  /** @brief Removes the pair under key by moving the last pair into its place; returns false when the key is absent. */
  bool Remove(const Key& key) noexcept {
    const std::size_t index = FindIndex(key);
    if (index == m_count) {
      return false;
    }
    RemoveAt(index);
    return true;
  }

  /** @brief Removes the pair at index by moving the last pair into its place; the order changes. */
  void RemoveAt(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    const std::size_t last = m_count - 1;
    if (index != last) {
      Replace(m_keys + index, std::move(m_keys[last]));
      Replace(m_values + index, std::move(m_values[last]));
    }
    std::destroy_at(m_keys + last);
    std::destroy_at(m_values + last);
    m_count = last;
  }

  /** @brief Destroys every pair. */
  void Clear() noexcept {
    while (m_count > 0) {
      --m_count;
      std::destroy_at(m_keys + m_count);
      std::destroy_at(m_values + m_count);
    }
  }

  /** @brief Returns the fixed number of pairs the map can hold. */
  [[nodiscard]] static constexpr std::size_t GetCapacity() noexcept { return Capacity; }

  /** @brief Returns the number of pairs held. */
  [[nodiscard]] std::size_t GetCount() const noexcept { return m_count; }

  /** @brief Checks whether no pair is held. */
  [[nodiscard]] bool IsEmpty() const noexcept { return m_count == 0; }

  /** @brief Checks whether the capacity is used up. */
  [[nodiscard]] bool IsFull() const noexcept { return m_count == Capacity; }

  /** @brief Returns the key at index, which must be below the count; keys never change while stored. */
  [[nodiscard]] const Key& GetKeyAt(std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_keys[index];
  }

  /** @brief Returns the value at index, which must be below the count. */
  [[nodiscard]] Value& GetValueAt(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_values[index];
  }

  /** @brief Returns the value at index, which must be below the count. */
  [[nodiscard]] const Value& GetValueAt(std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < m_count, "index out of range");
    return m_values[index];
  }

 private:
  [[nodiscard]] std::size_t FindIndex(const Key& key) const noexcept {
    for (std::size_t i = 0; i < m_count; ++i) {
      if (m_keys[i] == key) {
        return i;
      }
    }
    return m_count;
  }

  template <typename KeyArg, typename... Args>
  Value& Place(KeyArg&& key, Args&&... args) noexcept {
    std::construct_at(m_keys + m_count, std::forward<KeyArg>(key));
    Value& value = *std::construct_at(m_values + m_count, std::forward<Args>(args)...);
    ++m_count;
    return value;
  }

  template <typename Element, typename Source>
  static void Replace(Element* slot, Source&& source) noexcept {
    std::destroy_at(slot);
    std::construct_at(slot, std::forward<Source>(source));
  }

  void AppendCopies(const FlatMap& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_keys + i, other.m_keys[i]);
      std::construct_at(m_values + i, other.m_values[i]);
    }
    m_count = other.m_count;
  }

  void TakeOver(FlatMap& other) noexcept {
    for (std::size_t i = 0; i < other.m_count; ++i) {
      std::construct_at(m_keys + i, std::move(other.m_keys[i]));
      std::construct_at(m_values + i, std::move(other.m_values[i]));
    }
    m_count = other.m_count;
    other.Clear();
  }

  // Each union gets an active member, so the constexpr default constructor is a constant initializer.
  union {
    char m_unusedKeys;
    Key m_keys[Capacity];
  };
  union {
    char m_unusedValues;
    Value m_values[Capacity];
  };
  std::size_t m_count = 0;
};

}  // namespace CarPlay::System::Bedrock::Collections