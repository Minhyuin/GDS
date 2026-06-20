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

  APP::WGS::PROTOCOL::Result_s
  setSequence(OutputChannel_e _channel, std::span<const DacSample_s> _samples);

  std::span<const uint16_t> channelSamples(OutputChannel_e _channel) const;
  std::span<uint16_t> channelSamples(OutputChannel_e _channel);

private:
  ChannelPeriodBuffer_s<PERIOD_SAMPLE_COUNT> ch1_{};
  ChannelPeriodBuffer_s<PERIOD_SAMPLE_COUNT> ch2_{};
};

} // namespace APP::WGS::OUTPUT
