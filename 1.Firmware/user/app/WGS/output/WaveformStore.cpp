#include "WaveformStore.hpp"

#include <algorithm>

namespace APP::WGS::OUTPUT {

APP::WGS::PROTOCOL::Result_s
WaveformStore::setActiveLength(OutputChannel_e _channel, size_t _length) {
  if (!isValidLength(_length)) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
  }

  if (_channel == OutputChannel_e::CH1) {
    ch1ActiveLength_ = _length;
  } else {
    ch2ActiveLength_ = _length;
  }

  return APP::WGS::PROTOCOL::Result_s::success();
}

APP::WGS::PROTOCOL::Result_s
WaveformStore::setSequence(OutputChannel_e _channel,
                           std::span<const DacSample_s> _samples) {
  if (_samples.empty()) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
  }

  auto destination = channelSamples(_channel);
  const size_t copyCount = std::min(_samples.size(), activeLength(_channel));
  for (size_t i = 0; i < copyCount; ++i) {
    destination[i] = _samples[i].value;
  }

  const uint16_t last = destination[copyCount - 1U];
  for (size_t i = copyCount; i < destination.size(); ++i) {
    destination[i] = last;
  }

  return APP::WGS::PROTOCOL::Result_s::success();
}

size_t WaveformStore::activeLength(OutputChannel_e _channel) const {
  switch (_channel) {
  case OutputChannel_e::CH1:
    return ch1ActiveLength_;
  case OutputChannel_e::CH2:
    return ch2ActiveLength_;
  }

  return ch1ActiveLength_;
}

std::span<const uint16_t>
WaveformStore::channelSamples(OutputChannel_e _channel) const {
  switch (_channel) {
  case OutputChannel_e::CH1:
    return std::span<const uint16_t>(ch1_.samples.data(), activeLength(_channel));
  case OutputChannel_e::CH2:
    return std::span<const uint16_t>(ch2_.samples.data(), activeLength(_channel));
  }

  return std::span<const uint16_t>(ch1_.samples.data(), activeLength(_channel));
}

std::span<uint16_t> WaveformStore::channelSamples(OutputChannel_e _channel) {
  switch (_channel) {
  case OutputChannel_e::CH1:
    return std::span<uint16_t>(ch1_.samples.data(), activeLength(_channel));
  case OutputChannel_e::CH2:
    return std::span<uint16_t>(ch2_.samples.data(), activeLength(_channel));
  }

  return std::span<uint16_t>(ch1_.samples.data(), activeLength(_channel));
}

} // namespace APP::WGS::OUTPUT
