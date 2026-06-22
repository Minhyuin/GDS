#pragma once

#include "DacTypes.hpp"
#include "Result.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::OUTPUT {

class WaveformStore {
public:
  static constexpr size_t PERIOD_SAMPLE_COUNT = 1000;

  static constexpr bool isValidLength(size_t _length) {
    return _length > 0U && _length <= PERIOD_SAMPLE_COUNT;
  }

  APP::WGS::PROTOCOL::Result_s setActiveLength(OutputChannel_e _channel,
                                               size_t _length);

  APP::WGS::PROTOCOL::Result_s
  setSequence(OutputChannel_e _channel, std::span<const DacSample_s> _samples);

  size_t activeLength(OutputChannel_e _channel) const;

  std::span<const uint16_t> channelSamples(OutputChannel_e _channel) const;
  std::span<uint16_t> channelSamples(OutputChannel_e _channel);

private:
  ChannelPeriodBuffer_s<PERIOD_SAMPLE_COUNT> ch1_{};
  ChannelPeriodBuffer_s<PERIOD_SAMPLE_COUNT> ch2_{};
  size_t ch1ActiveLength_{PERIOD_SAMPLE_COUNT};
  size_t ch2ActiveLength_{PERIOD_SAMPLE_COUNT};
};

} // namespace APP::WGS::OUTPUT
