#pragma once

#include "Command.hpp"
#include "DacTypes.hpp"
#include "Result.hpp"
#include "WaveformStore.hpp"

#include <cmath>
#include <cstdint>
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
  static constexpr float MIN_POINT_PULSE_WIDTH_US = 1.0F;
  static constexpr float MAX_POINT_PULSE_WIDTH_US = 1000000.0F;

  DacWavePlayer() = default;
  ~DacWavePlayer() { stop(); }

  APP::WGS::PROTOCOL::Result_s start(WaveformStore &) {
    return enterIdleLow();
  }

  APP::WGS::PROTOCOL::Result_s enterIdleLow() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    clearPointPulseState();

    if (DacHardware::setDacValue(DAC_CHANNEL_1, 0U) != HAL_OK) {
      return fail();
    }
    if (DacHardware::setDacValue(DAC_CHANNEL_2, 0U) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }

    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void stop() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    clearPointPulseState();
    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
    state_ = PlayerState_e::STOPPED;
  }

  APP::WGS::PROTOCOL::Result_s reloadChannel(OutputChannel_e _channel,
                                             WaveformStore &_store) {
    if (isPointPulseBusy()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (stopChannel(_channel) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, false);

    if (startChannel(_channel, _store) != HAL_OK) {
      return fail();
    }

    setSequencePlaying(_channel, true);
    state_ = PlayerState_e::PLAYING;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s configureWaveform(OutputChannel_e _channel,
                                                 size_t _lengthSa,
                                                 float _frequencyHz,
                                                 WaveformStore &_store) {
    if (isPointPulseBusy()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (!WaveformStore::isValidLength(_lengthSa)) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    if (!std::isfinite(_frequencyHz) || _frequencyHz <= 0.0F) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    const float sampleRateHz = static_cast<float>(_lengthSa) * _frequencyHz;
    if (!std::isfinite(sampleRateHz) || sampleRateHz > MAX_SAMPLE_RATE_HZ) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    auto result = setSampleRate(_channel, sampleRateHz);
    if (!result.ok()) {
      return result;
    }
    return _store.setActiveLength(_channel, _lengthSa);
  }

  APP::WGS::PROTOCOL::Result_s sync(WaveformStore &_store) {
    if (isPointPulseBusy()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (!canSync()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

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
    const bool wasBusy = isPointPulseBusy();

    if (DacHardware::stopDma(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, false);
    if (DacHardware::setDacValue(dacChannel(_channel), _sample) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (wasBusy) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    if (preparePointPulse(_channel, _ackPulseUs) != HAL_OK) {
      return fail();
    }
    if (!armPointPulse(pointPulseMask(_channel))) {
      cancelPointPulse(pointPulseMask(_channel));
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s holdChannels(uint16_t _ch1Sample,
                                            uint16_t _ch2Sample,
                                            float _ackPulseUs) {
    const bool wasBusy = isPointPulseBusy();

    if (DacHardware::stopDma(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::stopDma(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    if (DacHardware::setDacValue(DAC_CHANNEL_1, _ch1Sample) != HAL_OK) {
      return fail();
    }
    if (DacHardware::setDacValue(DAC_CHANNEL_2, _ch2Sample) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }

    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
    if (wasBusy) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (preparePointPulse(OutputChannel_e::CH1, _ackPulseUs) != HAL_OK) {
      return fail();
    }
    if (preparePointPulse(OutputChannel_e::CH2, _ackPulseUs) != HAL_OK) {
      cancelPointPulse(POINT_PULSE_CH1);
      return fail();
    }
    if (!armPointPulse(POINT_PULSE_BOTH)) {
      cancelPointPulse(POINT_PULSE_BOTH);
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s setPointPulseWidth(float _pulseWidthUs) {
    if (isPointPulseBusy()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (!std::isfinite(_pulseWidthUs) ||
        _pulseWidthUs < APP::WGS::PROTOCOL::POINT_PULSE_WIDTH_MIN_US ||
        _pulseWidthUs > APP::WGS::PROTOCOL::POINT_PULSE_WIDTH_MAX_US) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    pointPulseWidthUs_ = _pulseWidthUs;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  float pointPulseWidthUs() const { return pointPulseWidthUs_; }

  APP::WGS::PROTOCOL::Result_s setSampleRate(OutputChannel_e _channel,
                                             float _sampleRateHz) {
    if (isPointPulseBusy()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

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
  bool canSync() const { return sequencePlayingCh1_ && sequencePlayingCh2_; }

  bool isPointPulseBusy() const {
    return (pendingPointPulseMask_ | firingPointPulseMask_) != 0U;
  }

  void firePendingPointPulseFromIsr() {
    uint8_t mask = POINT_PULSE_NONE;

    const auto irqState = __get_PRIMASK();
    __disable_irq();
    if (pendingPointPulseMask_ != POINT_PULSE_NONE &&
        firingPointPulseMask_ == POINT_PULSE_NONE) {
      mask = pendingPointPulseMask_;
      pendingPointPulseMask_ = POINT_PULSE_NONE;
      firingPointPulseMask_ = mask;
    }
    if (irqState == 0U) {
      __enable_irq();
    }

    if ((mask & POINT_PULSE_CH1) != 0U &&
        DMAch1Hardware::firePreparedSinglePulseFromIsr() != HAL_OK) {
      clearFiringPointPulseMask(POINT_PULSE_CH1);
    }
    if ((mask & POINT_PULSE_CH2) != 0U &&
        DMAch2Hardware::firePreparedSinglePulseFromIsr() != HAL_OK) {
      clearFiringPointPulseMask(POINT_PULSE_CH2);
    }
  }

  void completePointPulseFromIsr(OutputChannel_e _channel) {
    const uint8_t mask = pointPulseMask(_channel);
    const bool completed =
        _channel == OutputChannel_e::CH1
            ? DMAch1Hardware::completePreparedSinglePulseFromIsr()
            : DMAch2Hardware::completePreparedSinglePulseFromIsr();
    if (completed || (firingPointPulseMask_ & mask) != 0U) {
      clearFiringPointPulseMask(mask);
    }
  }

private:
  static constexpr uint8_t POINT_PULSE_NONE = 0x00U;
  static constexpr uint8_t POINT_PULSE_CH1 = 0x01U;
  static constexpr uint8_t POINT_PULSE_CH2 = 0x02U;
  static constexpr uint8_t POINT_PULSE_BOTH =
      POINT_PULSE_CH1 | POINT_PULSE_CH2;

  static uint32_t dacChannel(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  }

  static uint8_t pointPulseMask(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? POINT_PULSE_CH1
                                            : POINT_PULSE_CH2;
  }

  static HAL_StatusTypeDef startTimer(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::startContinuous()
                                            : DMAch2Hardware::startContinuous();
  }

  static HAL_StatusTypeDef stopTimer(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DMAch1Hardware::stopContinuous()
                                            : DMAch2Hardware::stopContinuous();
  }

  static HAL_StatusTypeDef preparePointPulse(OutputChannel_e _channel,
                                             float _ackPulseUs) {
    return _channel == OutputChannel_e::CH1
               ? DMAch1Hardware::prepareSinglePulse(_ackPulseUs)
               : DMAch2Hardware::prepareSinglePulse(_ackPulseUs);
  }

  static void cancelPointPulse(uint8_t _mask) {
    if ((_mask & POINT_PULSE_CH1) != 0U) {
      DMAch1Hardware::cancelPreparedSinglePulse();
    }
    if ((_mask & POINT_PULSE_CH2) != 0U) {
      DMAch2Hardware::cancelPreparedSinglePulse();
    }
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

  void setSequencePlaying(OutputChannel_e _channel, bool _playing) {
    if (_channel == OutputChannel_e::CH1) {
      sequencePlayingCh1_ = _playing;
    } else {
      sequencePlayingCh2_ = _playing;
    }
  }

  bool armPointPulse(uint8_t _mask) {
    const auto irqState = __get_PRIMASK();
    __disable_irq();
    const bool available = !isPointPulseBusy();
    if (available) {
      pendingPointPulseMask_ = _mask;
    }
    if (irqState == 0U) {
      __enable_irq();
    }
    return available;
  }

  void clearFiringPointPulseMask(uint8_t _mask) {
    const auto irqState = __get_PRIMASK();
    __disable_irq();
    firingPointPulseMask_ &= static_cast<uint8_t>(~_mask);
    if (firingPointPulseMask_ == POINT_PULSE_NONE &&
        pendingPointPulseMask_ == POINT_PULSE_NONE) {
      state_ = PlayerState_e::READY;
    }
    if (irqState == 0U) {
      __enable_irq();
    }
  }

  void clearPointPulseState() {
    cancelPointPulse(POINT_PULSE_BOTH);
    pendingPointPulseMask_ = POINT_PULSE_NONE;
    firingPointPulseMask_ = POINT_PULSE_NONE;
  }

  APP::WGS::PROTOCOL::Result_s fail() {
    state_ = PlayerState_e::FAULT;
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
  }

  PlayerState_e state_{PlayerState_e::STOPPED};
  bool sequencePlayingCh1_{false};
  bool sequencePlayingCh2_{false};
  float pointPulseWidthUs_{APP::WGS::PROTOCOL::POINT_PULSE_WIDTH_DEFAULT_US};
  volatile uint8_t pendingPointPulseMask_{POINT_PULSE_NONE};
  volatile uint8_t firingPointPulseMask_{POINT_PULSE_NONE};
};

} // namespace APP::WGS::OUTPUT
