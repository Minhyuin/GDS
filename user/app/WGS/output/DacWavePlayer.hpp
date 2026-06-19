#pragma once

#include "DacTypes.hpp"
#include "Result.hpp"
#include "WaveformStore.hpp"

#include <array>

namespace APP::WGS::OUTPUT {

template <typename DacHardware, typename TimerHardware> class DacWavePlayer {
public:
  static constexpr size_t DMA_HALF_FRAMES = 64;
  static constexpr size_t DMA_TOTAL_FRAMES = DMA_HALF_FRAMES * 2U;

  DacWavePlayer() = default;
  ~DacWavePlayer() { stop(); }

  APP::WGS::PROTOCOL::Result_s start(WaveformStore &_store) {
    refillHalf(BufferHalf_e::FIRST, _store);
    refillHalf(BufferHalf_e::SECOND, _store);

    if (DacHardware::startDma(DAC_CHANNEL_1, xDmaBuffer_.data(),
                              xDmaBuffer_.size()) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDma(DAC_CHANNEL_2, yDmaBuffer_.data(),
                              yDmaBuffer_.size()) != HAL_OK) {
      return fail();
    }
    if (TimerHardware::start() != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void stop() {
    DacHardware::stopDma(DAC_CHANNEL_1);
    DacHardware::stopDma(DAC_CHANNEL_2);
    TimerHardware::stop();
    state_ = PlayerState_e::STOPPED;
  }

  void refillHalf(BufferHalf_e _half, WaveformStore &_store) {
    const size_t offset =
        _half == BufferHalf_e::FIRST ? 0U : DMA_HALF_FRAMES;
    for (size_t i = 0; i < DMA_HALF_FRAMES; ++i) {
      DacFrame_s frame{};
      if (_store.popFromIsr(frame)) {
        lastFrame_ = frame;
      }
      xDmaBuffer_[offset + i] = lastFrame_.x;
      yDmaBuffer_[offset + i] = lastFrame_.y;
    }
    state_ = PlayerState_e::PLAYING;
  }

  APP::WGS::PROTOCOL::Result_s hold(DacFrame_s _frame) {
    if (DacHardware::setDacValue(DAC_CHANNEL_1, _frame.x) != HAL_OK) {
      return fail();
    }
    if (DacHardware::setDacValue(DAC_CHANNEL_2, _frame.y) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_1) != HAL_OK) {
      return fail();
    }
    if (DacHardware::startDac(DAC_CHANNEL_2) != HAL_OK) {
      return fail();
    }

    state_ = PlayerState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  PlayerState_e state() const { return state_; }

private:
  APP::WGS::PROTOCOL::Result_s fail() {
    state_ = PlayerState_e::FAULT;
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
  }

  std::array<uint32_t, DMA_TOTAL_FRAMES> xDmaBuffer_{};
  std::array<uint32_t, DMA_TOTAL_FRAMES> yDmaBuffer_{};
  DacFrame_s lastFrame_{};
  PlayerState_e state_{PlayerState_e::STOPPED};
};

} // namespace APP::WGS::OUTPUT
