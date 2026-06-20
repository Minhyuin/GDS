#include "WaveformStore.hpp"

namespace APP::WGS::OUTPUT {

APP::WGS::PROTOCOL::Result_s
WaveformStore::setSequence(OutputChannel_e _channel,
                           std::span<const DacSample_s> _samples) {
  if (_samples.empty() || _samples.size() > PERIOD_SAMPLE_COUNT) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
  }

  auto destination = channelSamples(_channel);
  for (size_t i = 0; i < _samples.size(); ++i) {
    destination[i] = _samples[i].value;
  }

  const uint16_t last = destination[_samples.size() - 1U];
  for (size_t i = _samples.size(); i < PERIOD_SAMPLE_COUNT; ++i) {
    destination[i] = last;
  }

  return APP::WGS::PROTOCOL::Result_s::success();
}

std::span<const uint16_t>
WaveformStore::channelSamples(OutputChannel_e _channel) const {
  switch (_channel) {
  case OutputChannel_e::CH1:
    return std::span<const uint16_t>(ch1_.samples.data(), ch1_.samples.size());
  case OutputChannel_e::CH2:
    return std::span<const uint16_t>(ch2_.samples.data(), ch2_.samples.size());
  }

  return std::span<const uint16_t>(ch1_.samples.data(), ch1_.samples.size());
}

std::span<uint16_t> WaveformStore::channelSamples(OutputChannel_e _channel) {
  switch (_channel) {
  case OutputChannel_e::CH1:
    return std::span<uint16_t>(ch1_.samples.data(), ch1_.samples.size());
  case OutputChannel_e::CH2:
    return std::span<uint16_t>(ch2_.samples.data(), ch2_.samples.size());
  }

  return std::span<uint16_t>(ch1_.samples.data(), ch1_.samples.size());
}

} // namespace APP::WGS::OUTPUT
