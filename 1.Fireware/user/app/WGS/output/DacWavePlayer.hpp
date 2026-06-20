#pragma once

#include "DacTypes.hpp"
#include "Result.hpp"
#include "WaveformStore.hpp"

#include <span>

namespace APP::WGS::OUTPUT {

template <typename DacHardware, typename DMAch1Hardware,
          typename DMAch2Hardware>
class DacWavePlayer {
public:
  static constexpr size_t PERIOD_SAMPLE_COUNT =
      WaveformStore::PERIOD_SAMPLE_COUNT;
  static constexpr float MIN_SAMPLE_RATE_HZ = 1.0F;
  static constexpr float MAX_SAMPLE_RATE_HZ = 1000000.0F;

  DacWavePlayer() = default;
  ~DacWavePlayer() { stop(); }

  APP::WGS::PROTOCOL::Result_s start(WaveformStore &_store) {
    if (startChannel(OutputChannel_e::CH1, _store) != HAL_OK) {
      return fail();
    }
    if (startChannel(OutputChannel_e::CH2, _store) != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void stop() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    DMAch1Hardware::stop();
    DMAch2Hardware::stop();
    state_ = PlayerState_e::STOPPED;
  }

  APP::WGS::PROTOCOL::Result_s reloadChannel(OutputChannel_e _channel,
                                             WaveformStore &_store) {
    if (stopChannel(_channel) != HAL_OK) {
      return fail();
    }

    if (startChannel(_channel, _store) != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::PLAYING;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s holdChannel(OutputChannel_e _channel,
                                           uint16_t _sample) {
    if (DacHardware::stopDma(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (DacHardware::setDacValue(dacChannel(_channel), _sample) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (startTimer(_channel) != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s setSampleRate(OutputChannel_e _channel,
                                             float _sampleRateHz) {
    if (_sampleRateHz < MIN_SAMPLE_RATE_HZ ||
        _sampleRateHz > MAX_SAMPLE_RATE_HZ) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    const auto status =
        _channel == OutputChannel_e::CH1
            ? DMAch1Hardware::setUpdateFrequencyHz(_sampleRateHz)
            : DMAch2Hardware::setUpdateFrequencyHz(_sampleRateHz);
    if (status != HAL_OK) {
      return fail();
    }

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  PlayerState_e state() const { return state_; }

private:
  static uint32_t dacChannel(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  }

  static HAL_StatusTypeDef startTimer(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::start()
                                            : DMAch2Hardware::start();
  }

  static HAL_StatusTypeDef stopTimer(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::stop()
                                            : DMAch2Hardware::stop();
  }

  HAL_StatusTypeDef startChannel(OutputChannel_e _channel,
                                 WaveformStore &_store) {
    auto samples = _store.channelSamples(_channel);
    const auto dacStatus =
        DacHardware::startDma(dacChannel(_channel), samples.data(),
                              static_cast<uint32_t>(samples.size()));
    if (dacStatus != HAL_OK) {
      return dacStatus;
    }
    return startTimer(_channel);
  }

  HAL_StatusTypeDef stopChannel(OutputChannel_e _channel) {
    const auto timerStatus = stopTimer(_channel);
    const auto dacStatus = DacHardware::stopDma(dacChannel(_channel));
    return dacStatus != HAL_OK ? dacStatus : timerStatus;
  }

  APP::WGS::PROTOCOL::Result_s fail() {
    state_ = PlayerState_e::FAULT;
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
  }

  PlayerState_e state_{PlayerState_e::STOPPED};
};

} // namespace APP::WGS::OUTPUT
