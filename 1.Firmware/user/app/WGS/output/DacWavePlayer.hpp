#pragma once

#include "Command.hpp"
#include "DacTypes.hpp"
#include "Result.hpp"
#include "WaveformStore.hpp"

#include <array>
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
  static constexpr size_t POINT_FIFO_CAPACITY = 1024;
  static constexpr size_t POINT2_FIFO_CAPACITY = 1024;

  DacWavePlayer() = default;
  ~DacWavePlayer() { stop(); }

  APP::WGS::PROTOCOL::Result_s start(WaveformStore &) { return enterIdleLow(); }

  APP::WGS::PROTOCOL::Result_s enterIdleLow() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    resetPointModes();

    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    // Write after disabling timer triggers so DHR transfers to DOR immediately.
    DacHardware::setDacValue(DAC_CHANNEL_1, 0U);
    DacHardware::setDacValue(DAC_CHANNEL_2, 0U);

    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void stop() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    resetPointModes();
    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
  }

  APP::WGS::PROTOCOL::Result_s
  replaceSequence(OutputChannel_e _channel,
                  std::span<const APP::WGS::PROTOCOL::DacSample_s> _samples,
                  WaveformStore &_store) {
    if (isPointPulseBusy() || isPointFifoActive() || isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    if (_samples.empty()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    if (stopChannel(_channel) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, false);
    auto result = _store.setSequence(_channel, _samples);
    if (!result.ok()) {
      return result;
    }
    if (startChannel(_channel, _store) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, true);
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s configureWaveform(OutputChannel_e _channel,
                                                 size_t _lengthSa,
                                                 float _frequencyHz,
                                                 WaveformStore &_store) {
    if (isPointPulseBusy() || isPointFifoActive() || isPoint2FifoActive()) {
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

    if (sampleRateHz < MIN_SAMPLE_RATE_HZ) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }
    const bool playing = sequencePlaying(_channel);
    if (playing && stopChannel(_channel) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, false);
    auto result = setSampleRate(_channel, sampleRateHz);
    if (!result.ok()) {
      return result;
    }
    result = _store.setActiveLength(_channel, _lengthSa);
    if (playing) {
      if (startChannel(_channel, _store) != HAL_OK) {
        return fail();
      }
      setSequencePlaying(_channel, true);
    }
    return result;
  }

  APP::WGS::PROTOCOL::Result_s sync(WaveformStore &_store) {
    if (isPointPulseBusy() || isPointFifoActive() || isPoint2FifoActive()) {
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

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s
  holdChannel(OutputChannel_e _channel, uint16_t _sample, float _ackPulseUs) {
    if (isPointFifoActive() || isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    const bool wasBusy = isPointPulseBusy();
    if (sequencePlaying(_channel)) {
      stopTimer(_channel);
    }

    if (DacHardware::stopDma(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    setSequencePlaying(_channel, false);
    if (DacHardware::startDac(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    DacHardware::setDacValue(dacChannel(_channel), _sample);
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

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s
  holdChannels(uint16_t _ch1Sample, uint16_t _ch2Sample, float _ackPulseUs) {
    if (isPointFifoActive() || isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    const bool wasBusy = isPointPulseBusy();

    if (DacHardware::stopDma(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::stopDma(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    if (sequencePlayingCh1_) {
      DMAch1Hardware::stopContinuous();
    }
    if (sequencePlayingCh2_) {
      DMAch2Hardware::stopContinuous();
    }
    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    // Write after disabling timer triggers so DHR transfers to DOR immediately.
    DacHardware::setDacValue(DAC_CHANNEL_1, _ch1Sample);
    DacHardware::setDacValue(DAC_CHANNEL_2, _ch2Sample);

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

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s setPointPulseWidth(float _pulseWidthUs) {
    if (isPointPulseBusy() || isPointFifoActive() || isPoint2FifoActive()) {
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
    if (isPointPulseBusy() || isPointFifoActive() || isPoint2FifoActive()) {
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

  bool canSync() const { return sequencePlayingCh1_ && sequencePlayingCh2_; }

  bool isPointPulseBusy() const {
    return (pendingPointPulseMask_ | firingPointPulseMask_) != 0U;
  }

  bool isPointFifoActive() const {
    return pointFifoEnabled_ || pointFifoCount_ != 0U;
  }

  bool isPoint2FifoActive() const {
    return point2FifoEnabled_ || point2FifoCount_ != 0U;
  }

  APP::WGS::PROTOCOL::Result_s clearPointFifo() {
    if (isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    const auto irqState = __get_PRIMASK();
    __disable_irq();
    pointFifoEnabled_ = false;
    clearPointPulseState();
    pointFifoHead_ = 0;
    pointFifoTail_ = 0;
    pointFifoCount_ = 0;
    if (irqState == 0U) {
      __enable_irq();
    }
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s clearPointFifo(OutputChannel_e _channel) {
    if (isPointFifoActive() && _channel != pointFifoChannel_) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    return clearPointFifo();
  }

  APP::WGS::PROTOCOL::Result_s stopPointFifo(OutputChannel_e _channel) {
    return clearPointFifo(_channel);
  }

  APP::WGS::PROTOCOL::Result_s appendPointFifo(
      OutputChannel_e _channel,
      std::span<const APP::WGS::PROTOCOL::PointFifoPoint_s> _points) {
    if (_points.empty() || _points.size() > POINT_FIFO_CAPACITY) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    if (isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    size_t writeIndex = 0;
    {
      const auto irqState = __get_PRIMASK();
      __disable_irq();
      if (isPointFifoActive() && _channel != pointFifoChannel_) {
        if (irqState == 0U) {
          __enable_irq();
        }
        return APP::WGS::PROTOCOL::Result_s::busy();
      }
      const size_t freeCount = POINT_FIFO_CAPACITY - pointFifoCount_;
      if (_points.size() > freeCount) {
        if (irqState == 0U) {
          __enable_irq();
        }
        return APP::WGS::PROTOCOL::Result_s::busy();
      }
      pointFifoChannel_ = _channel;
      writeIndex = pointFifoTail_;
      if (irqState == 0U) {
        __enable_irq();
      }
    }

    for (auto point : _points) {
      pointFifo_[writeIndex] = point;
      writeIndex = nextPointFifoIndex(writeIndex);
    }

    const auto irqState = __get_PRIMASK();
    __disable_irq();
    pointFifoTail_ = writeIndex;
    pointFifoCount_ = pointFifoCount_ + _points.size();
    if (irqState == 0U) {
      __enable_irq();
    }

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s startPointFifo(OutputChannel_e _channel) {
    if (isPointPulseBusy() || isPoint2FifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    if (isPointFifoActive() && pointFifoChannel_ != _channel) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (pointFifoEnabled_) {
      return APP::WGS::PROTOCOL::Result_s::success();
    }
    if (DacHardware::stopDma(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    stopTimer(_channel);
    setSequencePlaying(_channel, false);
    if (DacHardware::startDac(dacChannel(_channel)) != HAL_OK) {
      return fail();
    }
    if (preparePointPulse(_channel, pointPulseWidthUs_) != HAL_OK) {
      return fail();
    }

    pointFifoChannel_ = _channel;
    pointFifoEnabled_ = true;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s clearPoint2Fifo() {
    if (isPointFifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }
    const auto irqState = __get_PRIMASK();
    __disable_irq();
    point2FifoEnabled_ = false;
    clearPointPulseState();
    point2FifoHead_ = 0;
    point2FifoTail_ = 0;
    point2FifoCount_ = 0;
    if (irqState == 0U) {
      __enable_irq();
    }
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s stopPoint2Fifo() { return clearPoint2Fifo(); }

  APP::WGS::PROTOCOL::Result_s appendPoint2Fifo(
      std::span<const APP::WGS::PROTOCOL::Point2FifoPoint_s> _points) {
    if (_points.empty() || _points.size() > POINT2_FIFO_CAPACITY) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    if (isPointFifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    size_t writeIndex = 0;
    {
      const auto irqState = __get_PRIMASK();
      __disable_irq();
      const size_t freeCount = POINT2_FIFO_CAPACITY - point2FifoCount_;
      if (_points.size() > freeCount) {
        if (irqState == 0U) {
          __enable_irq();
        }
        return APP::WGS::PROTOCOL::Result_s::busy();
      }
      writeIndex = point2FifoTail_;
      if (irqState == 0U) {
        __enable_irq();
      }
    }

    for (auto point : _points) {
      point2Fifo_[writeIndex] = point;
      writeIndex = nextPoint2FifoIndex(writeIndex);
    }

    const auto irqState = __get_PRIMASK();
    __disable_irq();
    point2FifoTail_ = writeIndex;
    point2FifoCount_ = point2FifoCount_ + _points.size();
    if (irqState == 0U) {
      __enable_irq();
    }

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s startPoint2Fifo() {
    if (isPointPulseBusy() || isPointFifoActive()) {
      return APP::WGS::PROTOCOL::Result_s::busy();
    }

    if (point2FifoEnabled_) {
      return APP::WGS::PROTOCOL::Result_s::success();
    }
    if (DacHardware::stopDma(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::stopDma(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    DMAch1Hardware::stopContinuous();
    DMAch2Hardware::stopContinuous();
    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }
    if (preparePointPulse(OutputChannel_e::CH1, pointPulseWidthUs_) != HAL_OK) {
      return fail();
    }
    if (preparePointPulse(OutputChannel_e::CH2, pointPulseWidthUs_) != HAL_OK) {
      cancelPointPulse(POINT_PULSE_CH1);
      return fail();
    }

    sequencePlayingCh1_ = false;
    sequencePlayingCh2_ = false;
    point2FifoEnabled_ = true;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  __attribute__((always_inline)) void firePendingPointPulseFromIsr() {
    // Tasks publish pending under PRIMASK; they cannot mutate it during EXTI.
    // Completion IRQs have the same priority, so they cannot preempt this path.
    const uint8_t mask = pendingPointPulseMask_;
    if (mask != POINT_PULSE_NONE) {
      if (mask == POINT_PULSE_BOTH) {
        DMAch1Hardware::raiseArmedAckFromIsr();
        DMAch2Hardware::raiseArmedAckFromIsr();
      } else if (mask == POINT_PULSE_CH1) {
        DMAch1Hardware::raiseArmedAckFromIsr();
      } else {
        DMAch2Hardware::raiseArmedAckFromIsr();
      }
      finishArmedPointPulseFromIsr(mask);
      return;
    }
    fireFifoPointPulseFromIsr();
  }

  void completePointPulseFromIsr(OutputChannel_e _channel) {
    const uint8_t mask = pointPulseMask(_channel);
    const bool completed =
        _channel == OutputChannel_e::CH1
            ? DMAch1Hardware::completePreparedSinglePulseFromIsr(
                  point2FifoEnabled_ || pointFifoEnabled_)
            : DMAch2Hardware::completePreparedSinglePulseFromIsr(
                  point2FifoEnabled_ || pointFifoEnabled_);
    if (completed || (firingPointPulseMask_ & mask) != 0U) {
      clearFiringPointPulseMask(mask);
    }
  }

private:
  static constexpr uint8_t POINT_PULSE_NONE = 0x00U;
  static constexpr uint8_t POINT_PULSE_CH1 = 0x01U;
  static constexpr uint8_t POINT_PULSE_CH2 = 0x02U;
  static constexpr uint8_t POINT_PULSE_BOTH = POINT_PULSE_CH1 | POINT_PULSE_CH2;

  static size_t nextPointFifoIndex(size_t _index) {
    ++_index;
    return _index >= POINT_FIFO_CAPACITY ? 0U : _index;
  }

  static size_t nextPoint2FifoIndex(size_t _index) {
    ++_index;
    return _index >= POINT2_FIFO_CAPACITY ? 0U : _index;
  }

  static uint32_t dacChannel(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
  }

  static uint8_t pointPulseMask(OutputChannel_e _channel) {
    return _channel == OutputChannel_e::CH1 ? POINT_PULSE_CH1 : POINT_PULSE_CH2;
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

  bool sequencePlaying(OutputChannel_e _channel) const {
    return _channel == OutputChannel_e::CH1 ? sequencePlayingCh1_
                                            : sequencePlayingCh2_;
  }

  void resetPointModes() {
    const auto irqState = __get_PRIMASK();
    __disable_irq();
    pointFifoEnabled_ = false;
    point2FifoEnabled_ = false;
    clearPointPulseState();
    pointFifoHead_ = 0;
    pointFifoTail_ = 0;
    pointFifoCount_ = 0;
    point2FifoHead_ = 0;
    point2FifoTail_ = 0;
    point2FifoCount_ = 0;
    if (irqState == 0U) {
      __enable_irq();
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
    if (irqState == 0U) {
      __enable_irq();
    }
  }

  void clearPointPulseState() {
    cancelPointPulse(POINT_PULSE_BOTH);
    pendingPointPulseMask_ = POINT_PULSE_NONE;
    firingPointPulseMask_ = POINT_PULSE_NONE;
  }

  // Run after the rising edges, keeping register saves out of the fast entry.
  __attribute__((noinline)) void finishArmedPointPulseFromIsr(uint8_t _mask) {
    pendingPointPulseMask_ = POINT_PULSE_NONE;
    firingPointPulseMask_ = _mask;
    if ((_mask & POINT_PULSE_CH1) != 0U) {
      DMAch1Hardware::startArmedPulseTimerFromIsr();
    }
    if ((_mask & POINT_PULSE_CH2) != 0U) {
      DMAch2Hardware::startArmedPulseTimerFromIsr();
    }
  }

  __attribute__((noinline)) void fireFifoPointPulseFromIsr() {
    if (!firePoint2FifoFromIsr()) {
      firePointFifoFromIsr();
    }
  }

  bool popPointFifoFromIsr(APP::WGS::PROTOCOL::PointFifoPoint_s &_point) {
    if (pointFifoCount_ == 0U) {
      return false;
    }

    _point = pointFifo_[pointFifoHead_];
    pointFifoHead_ = nextPointFifoIndex(pointFifoHead_);
    pointFifoCount_ = pointFifoCount_ - 1U;
    return true;
  }

  bool firePointFifoFromIsr() {
    if (!pointFifoEnabled_) {
      return false;
    }
    if (firingPointPulseMask_ != POINT_PULSE_NONE) {
      return true;
    }

    APP::WGS::PROTOCOL::PointFifoPoint_s point{};
    if (!popPointFifoFromIsr(point)) {
      return true;
    }

    const uint32_t dac = dacChannel(pointFifoChannel_);
    DacHardware::setDacValue(dac, point.sample.value);

    const uint8_t mask = pointPulseMask(pointFifoChannel_);
    firingPointPulseMask_ = mask;
    const HAL_StatusTypeDef status =
        pointFifoChannel_ == OutputChannel_e::CH1
            ? DMAch1Hardware::firePreparedSinglePulseFromIsr()
            : DMAch2Hardware::firePreparedSinglePulseFromIsr();
    if (status != HAL_OK) {
      clearFiringPointPulseMask(mask);
    }
    return true;
  }

  bool popPoint2FifoFromIsr(APP::WGS::PROTOCOL::Point2FifoPoint_s &_point) {
    if (point2FifoCount_ == 0U) {
      return false;
    }

    _point = point2Fifo_[point2FifoHead_];
    point2FifoHead_ = nextPoint2FifoIndex(point2FifoHead_);
    point2FifoCount_ = point2FifoCount_ - 1U;
    return true;
  }

  bool firePoint2FifoFromIsr() {
    if (!point2FifoEnabled_) {
      return false;
    }
    if (firingPointPulseMask_ != POINT_PULSE_NONE) {
      return true;
    }

    APP::WGS::PROTOCOL::Point2FifoPoint_s point{};
    if (!popPoint2FifoFromIsr(point)) {
      return true;
    }

    DacHardware::setDacValue(DAC_CHANNEL_1, point.ch1Sample.value);
    DacHardware::setDacValue(DAC_CHANNEL_2, point.ch2Sample.value);

    firingPointPulseMask_ = POINT_PULSE_BOTH;
    if (DMAch1Hardware::firePreparedSinglePulseFromIsr() != HAL_OK) {
      clearFiringPointPulseMask(POINT_PULSE_CH1);
    }
    if (DMAch2Hardware::firePreparedSinglePulseFromIsr() != HAL_OK) {
      clearFiringPointPulseMask(POINT_PULSE_CH2);
    }
    return true;
  }

  APP::WGS::PROTOCOL::Result_s fail() {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
  }

  bool sequencePlayingCh1_{false};
  bool sequencePlayingCh2_{false};
  float pointPulseWidthUs_{APP::WGS::PROTOCOL::POINT_PULSE_WIDTH_DEFAULT_US};
  volatile uint8_t pendingPointPulseMask_{POINT_PULSE_NONE};
  volatile uint8_t firingPointPulseMask_{POINT_PULSE_NONE};
  std::array<APP::WGS::PROTOCOL::PointFifoPoint_s, POINT_FIFO_CAPACITY>
      pointFifo_{};
  volatile bool pointFifoEnabled_{false};
  OutputChannel_e pointFifoChannel_{OutputChannel_e::CH1};
  volatile size_t pointFifoHead_{0};
  volatile size_t pointFifoTail_{0};
  volatile size_t pointFifoCount_{0};
  std::array<APP::WGS::PROTOCOL::Point2FifoPoint_s, POINT2_FIFO_CAPACITY>
      point2Fifo_{};
  volatile bool point2FifoEnabled_{false};
  volatile size_t point2FifoHead_{0};
  volatile size_t point2FifoTail_{0};
  volatile size_t point2FifoCount_{0};
};

} // namespace APP::WGS::OUTPUT
