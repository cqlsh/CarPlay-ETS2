#pragma once

#include "system/Bedrock/Assert.hpp"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace CarPlay::System::Bedrock::Collections {

/** @brief Yields a zero of the narrowest unsigned type that holds BitCount bits, or of uint64_t beyond 32 bits. */
template <std::size_t BitCount>
[[nodiscard]] constexpr auto PickBitMaskWord() noexcept {
  if constexpr (BitCount <= 8) {
    return std::uint8_t{0};
  } else if constexpr (BitCount <= 16) {
    return std::uint16_t{0};
  } else if constexpr (BitCount <= 32) {
    return std::uint32_t{0};
  } else {
    return std::uint64_t{0};
  }
}

/** @brief The word type a BitMask with BitCount bits stores its bits in. */
template <std::size_t BitCount>
using BitMaskWord = decltype(PickBitMaskWord<BitCount>());

/**
 * @brief Holds a fixed number of bits in unsigned words, in place, with every operation usable at compile time.
 * @details Bits are numbered from 0 and the mask doubles as a set of those numbers: iterating it visits the indices
 *          of the set bits in ascending order. Bits past BitCount stay zero in the last word, so masks with the same
 *          bits compare equal. Meant for flags, dirty rows, pressed keys and similar. Not synchronized.
 */
template <std::size_t BitCount>
requires (BitCount > 0)
class BitMask {
 public:
  using Word = BitMaskWord<BitCount>;

  static constexpr std::size_t WordBits = std::numeric_limits<Word>::digits;
  static constexpr std::size_t WordCount = (BitCount + WordBits - 1) / WordBits;

  /** @brief Walks the indices of the set bits in ascending order; what begin and end hand out. */
  class SetBitIterator {
   public:
    constexpr SetBitIterator(const Word* words, std::size_t wordIndex) noexcept
        : m_words(words),
          m_wordIndex(wordIndex) {
      SkipEmptyWords();
    }

    /** @brief Returns the index of the set bit the iterator stands on, which must not be the end. */
    [[nodiscard]] constexpr std::size_t operator*() const noexcept {
      CARPLAY_ASSERT(m_remaining != 0, "iterator at end");
      return m_wordIndex * WordBits + static_cast<std::size_t>(std::countr_zero(m_remaining));
    }

    /** @brief Moves on to the next set bit, which may be the end. */
    constexpr SetBitIterator& operator++() noexcept {
      CARPLAY_ASSERT(m_remaining != 0, "iterator at end");
      m_remaining = static_cast<Word>(m_remaining & (m_remaining - 1));
      if (m_remaining == 0) {
        ++m_wordIndex;
        SkipEmptyWords();
      }
      return *this;
    }

    [[nodiscard]] constexpr bool operator==(const SetBitIterator& other) const noexcept = default;

   private:
    constexpr void SkipEmptyWords() noexcept {
      while (m_wordIndex < WordCount && m_words[m_wordIndex] == 0) {
        ++m_wordIndex;
      }
      m_remaining = m_wordIndex < WordCount ? m_words[m_wordIndex] : Word{0};
    }

    const Word* m_words;
    std::size_t m_wordIndex;
    Word m_remaining = 0;
  };

  constexpr BitMask() noexcept = default;

  /** @brief Returns a mask with the given bits set; each index must be below the capacity. */
  template <std::convertible_to<std::size_t>... Index>
  [[nodiscard]] static constexpr BitMask Of(Index... indices) noexcept {
    BitMask mask;
    (mask.Set(static_cast<std::size_t>(indices)), ...);
    return mask;
  }

  /** @brief Sets the bit at index, which must be below the capacity. */
  constexpr void Set(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < BitCount, "index out of range");
    m_words[index / WordBits] = static_cast<Word>(m_words[index / WordBits] | Bit(index));
  }

  /** @brief Sets or clears the bit at index, which must be below the capacity. */
  constexpr void Set(std::size_t index, bool value) noexcept {
    if (value) {
      Set(index);
    } else {
      Clear(index);
    }
  }

  /** @brief Clears the bit at index, which must be below the capacity. */
  constexpr void Clear(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < BitCount, "index out of range");
    m_words[index / WordBits] = static_cast<Word>(m_words[index / WordBits] & ~Bit(index));
  }

  /** @brief Flips the bit at index, which must be below the capacity. */
  constexpr void Flip(std::size_t index) noexcept {
    CARPLAY_ASSERT(index < BitCount, "index out of range");
    m_words[index / WordBits] = static_cast<Word>(m_words[index / WordBits] ^ Bit(index));
  }

  /** @brief Sets every bit. */
  constexpr void SetAll() noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = FullWord(i);
    }
  }

  /** @brief Clears every bit. */
  constexpr void ClearAll() noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = 0;
    }
  }

  /** @brief Flips every bit. */
  constexpr void FlipAll() noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = static_cast<Word>(~m_words[i] & FullWord(i));
    }
  }

  /** @brief Stores word at wordIndex, which must be below the word count; bits past the capacity are dropped. */
  constexpr void SetWord(std::size_t wordIndex, Word word) noexcept {
    CARPLAY_ASSERT(wordIndex < WordCount, "word index out of range");
    m_words[wordIndex] = static_cast<Word>(word & FullWord(wordIndex));
  }

  /** @brief Keeps only the bits that are set in both masks. */
  constexpr BitMask& operator&=(const BitMask& other) noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = static_cast<Word>(m_words[i] & other.m_words[i]);
    }
    return *this;
  }

  /** @brief Adds the bits that are set in other. */
  constexpr BitMask& operator|=(const BitMask& other) noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = static_cast<Word>(m_words[i] | other.m_words[i]);
    }
    return *this;
  }

  /** @brief Flips the bits that are set in other. */
  constexpr BitMask& operator^=(const BitMask& other) noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      m_words[i] = static_cast<Word>(m_words[i] ^ other.m_words[i]);
    }
    return *this;
  }

  [[nodiscard]] friend constexpr BitMask operator&(BitMask left, const BitMask& right) noexcept { return left &= right; }
  [[nodiscard]] friend constexpr BitMask operator|(BitMask left, const BitMask& right) noexcept { return left |= right; }
  [[nodiscard]] friend constexpr BitMask operator^(BitMask left, const BitMask& right) noexcept { return left ^= right; }

  /** @brief Returns a copy with every bit flipped. */
  [[nodiscard]] constexpr BitMask operator~() const noexcept {
    BitMask result = *this;
    result.FlipAll();
    return result;
  }

  [[nodiscard]] constexpr bool operator==(const BitMask& other) const noexcept = default;

  /** @brief Checks whether the bit at index, which must be below the capacity, is set. */
  [[nodiscard]] constexpr bool Has(std::size_t index) const noexcept {
    CARPLAY_ASSERT(index < BitCount, "index out of range");
    return (m_words[index / WordBits] & Bit(index)) != 0;
  }

  /** @brief Checks whether at least one bit set in other is set here too. */
  [[nodiscard]] constexpr bool HasAny(const BitMask& other) const noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      if ((m_words[i] & other.m_words[i]) != 0) {
        return true;
      }
    }
    return false;
  }

  /** @brief Checks whether every bit set in other is set here too. */
  [[nodiscard]] constexpr bool HasAll(const BitMask& other) const noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      if ((m_words[i] & other.m_words[i]) != other.m_words[i]) {
        return false;
      }
    }
    return true;
  }

  /** @brief Returns the fixed number of bits the mask holds. */
  [[nodiscard]] static constexpr std::size_t GetCapacity() noexcept { return BitCount; }

  /** @brief Returns the number of set bits. */
  [[nodiscard]] constexpr std::size_t GetCount() const noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < WordCount; ++i) {
      count += static_cast<std::size_t>(std::popcount(m_words[i]));
    }
    return count;
  }

  /** @brief Checks whether no bit is set. */
  [[nodiscard]] constexpr bool IsEmpty() const noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      if (m_words[i] != 0) {
        return false;
      }
    }
    return true;
  }

  /** @brief Checks whether every bit is set. */
  [[nodiscard]] constexpr bool IsFull() const noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      if (m_words[i] != FullWord(i)) {
        return false;
      }
    }
    return true;
  }

  /** @brief Finds the lowest set bit; returns false and leaves index alone when no bit is set. */
  [[nodiscard]] constexpr bool TryFindLowest(std::size_t& index) const noexcept {
    for (std::size_t i = 0; i < WordCount; ++i) {
      if (m_words[i] != 0) {
        index = i * WordBits + static_cast<std::size_t>(std::countr_zero(m_words[i]));
        return true;
      }
    }
    return false;
  }

  /** @brief Finds the highest set bit; returns false and leaves index alone when no bit is set. */
  [[nodiscard]] constexpr bool TryFindHighest(std::size_t& index) const noexcept {
    for (std::size_t i = WordCount; i > 0; --i) {
      if (m_words[i - 1] != 0) {
        index = (i - 1) * WordBits + (WordBits - 1 - static_cast<std::size_t>(std::countl_zero(m_words[i - 1])));
        return true;
      }
    }
    return false;
  }

  /** @brief Returns the word at wordIndex, which must be below the word count; bit 0 of word 0 is bit 0 of the mask. */
  [[nodiscard]] constexpr Word GetWord(std::size_t wordIndex) const noexcept {
    CARPLAY_ASSERT(wordIndex < WordCount, "word index out of range");
    return m_words[wordIndex];
  }

  [[nodiscard]] constexpr SetBitIterator begin() const noexcept { return SetBitIterator(m_words, 0); }
  [[nodiscard]] constexpr SetBitIterator end() const noexcept { return SetBitIterator(m_words, WordCount); }

 private:
  [[nodiscard]] static constexpr Word Bit(std::size_t index) noexcept {
    return static_cast<Word>(Word{1} << (index % WordBits));
  }

  // The last word may have bits past BitCount; this masks them off so they stay zero and comparisons stay honest.
  [[nodiscard]] static constexpr Word FullWord(std::size_t wordIndex) noexcept {
    constexpr std::size_t lastWordBits = BitCount % WordBits;
    if (wordIndex + 1 < WordCount || lastWordBits == 0) {
      return std::numeric_limits<Word>::max();
    }
    return static_cast<Word>((Word{1} << lastWordBits) - 1);
  }

  Word m_words[WordCount] = {};
};

}  // namespace CarPlay::System::Bedrock::Collections