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
  static constexpr float MAX_SAMPLE_RATE_HZ = 100000.0F; // 100k Sa/s

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
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
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

  APP::WGS::PROTOCOL::Result_s sync(WaveformStore &_store) {
    const auto irqState = __get_PRIMASK();
    __disable_irq();

    HAL_StatusTypeDef status = stopBothTimers();
    if (status == HAL_OK) {
      status = stopBothDma();
    }
    if (status == HAL_OK) {
      status = armDma(OutputChannel_e::CH1, _store);
    }
    if (status == HAL_OK) {
      status = armDma(OutputChannel_e::CH2, _store);
    }
    if (status == HAL_OK) {
      resetBothTimers();
      status = startBothTimers();
    }

    if (irqState == 0U) {
      __enable_irq();
    }

    if (status != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::PLAYING;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s holdChannel(OutputChannel_e _channel,
                                           uint16_t _sample,
                                           float _ackPulseUs) {
    if (DacHardware::stopDma(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (DacHardware::setDacValue(dacChannel(_channel), _sample) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (pulseTimer(_channel, _ackPulseUs) != HAL_OK) {
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
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::startContinuous()
                                            : DMAch2Hardware::startContinuous();
  }

  static HAL_StatusTypeDef stopTimer(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::stopContinuous()
                                            : DMAch2Hardware::stopContinuous();
  }

  static HAL_StatusTypeDef pulseTimer(OutputChannel_e _channel,
                                      float _ackPulseUs) {
    return _channel == OutputChannel_e::CH1
               ? DMAch1Hardware::pulseOnce(_ackPulseUs)
               : DMAch2Hardware::pulseOnce(_ackPulseUs);
  }

  HAL_StatusTypeDef startChannel(OutputChannel_e _channel,
                                 WaveformStore &_store) {
    const auto dacStatus = armDma(_channel, _store);
    if (dacStatus != HAL_OK) {
      return dacStatus;
    }
    return startTimer(_channel);
  }

  HAL_StatusTypeDef armDma(OutputChannel_e _channel, WaveformStore &_store) {
    auto samples = _store.channelSamples(_channel);
    return DacHardware::startDma(dacChannel(_channel), samples.data(),
                                 static_cast<uint32_t>(samples.size()));
  }

  HAL_StatusTypeDef stopChannel(OutputChannel_e _channel) {
    const auto timerStatus = stopTimer(_channel);
    const auto dacStatus = DacHardware::stopDma(dacChannel(_channel));
    return dacStatus != HAL_OK ? dacStatus : timerStatus;
  }

  HAL_StatusTypeDef stopBothTimers() {
    const auto ch1Status = stopTimer(OutputChannel_e::CH1);
    const auto ch2Status = stopTimer(OutputChannel_e::CH2);
    return ch1Status != HAL_OK ? ch1Status : ch2Status;
  }

  HAL_StatusTypeDef stopBothDma() {
    const auto ch1Status = DacHardware::stopDma(DAC_CHANNEL_1);
    const auto ch2Status = DacHardware::stopDma(DAC_CHANNEL_2);
    return ch1Status != HAL_OK ? ch1Status : ch2Status;
  }

  static void resetBothTimers() {
    DMAch1Hardware::resetCounter();
    DMAch2Hardware::resetCounter();
  }

  HAL_StatusTypeDef startBothTimers() {
    DMAch1Hardware::startCounterNow();
    DMAch2Hardware::startCounterNow();
    return HAL_OK;
  }

  APP::WGS::PROTOCOL::Result_s fail() {
    state_ = PlayerState_e::FAULT;
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
  }

  PlayerState_e state_{PlayerState_e::STOPPED};
};

} // namespace APP::WGS::OUTPUT
