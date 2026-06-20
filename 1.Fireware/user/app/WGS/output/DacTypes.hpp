#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::OUTPUT {

enum class OutputChannel_e : uint8_t {
  CH1 = 0x01,
  CH2 = 0x02,
};

enum class PlayerState_e : uint8_t {
  STOPPED,
  READY,
  PLAYING,
  FAULT,
};

struct DacSample_s {
  uint16_t value{0};
};

template <size_t SampleCount> struct ChannelPeriodBuffer_s {
  static constexpr size_t SAMPLE_COUNT = SampleCount;
  std::array<uint16_t, SAMPLE_COUNT> samples{};

  void fill(uint16_t _value) {
    samples.fill(_value);
  }
};

} // namespace APP::WGS::OUTPUT
