#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::OUTPUT {

enum class BufferHalf_e : uint8_t {
  FIRST,
  SECOND,
};

enum class PlayerState_e : uint8_t {
  STOPPED,
  READY,
  PLAYING,
  FAULT,
};

struct DacFrame_s {
  uint16_t x{0};
  uint16_t y{0};
};

template <size_t HalfFrames> struct DacDoubleBuffer_s {
  static constexpr size_t HALF_FRAMES = HalfFrames;
  static constexpr size_t TOTAL_FRAMES = HalfFrames * 2U;

  std::array<DacFrame_s, TOTAL_FRAMES> frames{};

  std::span<DacFrame_s> half(BufferHalf_e _half) {
    const size_t offset = _half == BufferHalf_e::FIRST ? 0 : HALF_FRAMES;
    return std::span<DacFrame_s>(frames.data() + offset, HALF_FRAMES);
  }
};

} // namespace APP::WGS::OUTPUT
