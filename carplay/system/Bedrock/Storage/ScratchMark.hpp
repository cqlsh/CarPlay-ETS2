#pragma once

#include "system/Bedrock/Storage/FrameScratch.hpp"

#include <cstddef>

namespace CarPlay::System::Bedrock::Storage {

/**
 * @brief Remembers the cursor of a FrameScratch and rewinds to it when the scope ends.
 * @details Bytes taken inside the scope are free again afterwards, so pointers to them die with the mark.
 *          Marks nest in scope order, the inner one ends first. The scratch must outlive the mark. Not synchronized.
 */
class [[nodiscard]] ScratchMark {
 public:
  /** @brief Captures the current cursor of scratch. */
  explicit ScratchMark(FrameScratch& scratch) noexcept
      : m_scratch(&scratch),
        m_cursor(scratch.GetCursor()) {}

  ScratchMark(const ScratchMark&) = delete;
  ScratchMark& operator=(const ScratchMark&) = delete;
  ScratchMark(ScratchMark&&) = delete;
  ScratchMark& operator=(ScratchMark&&) = delete;

  ~ScratchMark() noexcept {
    if (m_scratch != nullptr) {
      m_scratch->Rewind(m_cursor);
    }
  }

  /** @brief Leaves the bytes taken inside the scope in place; the mark no longer rewinds. */
  void Keep() noexcept { m_scratch = nullptr; }

  /** @brief Returns the cursor the mark rewinds to. */
  [[nodiscard]] std::size_t GetCursor() const noexcept { return m_cursor; }

 private:
  FrameScratch* m_scratch;
  std::size_t m_cursor;
};

}  // namespace CarPlay::System::Bedrock::Storage