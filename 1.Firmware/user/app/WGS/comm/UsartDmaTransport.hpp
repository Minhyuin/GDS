#pragma once

#include "Result.hpp"
#include "TransportTypes.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace APP::WGS::COMM {

enum class TransportState_e : uint8_t {
  STOPPED,
  READY,
  FAULT,
};

template <typename Proxy> class UsartDmaTransport {
public:
  static constexpr size_t RX_BUFFER_SIZE = 8192;
  static constexpr size_t RX_SCRATCH_SIZE = 256;
  static constexpr uint32_t TX_TIMEOUT_MS = 10;

  UsartDmaTransport() = default;
  ~UsartDmaTransport() { stop(); }

  APP::WGS::PROTOCOL::Result_s start() { return restart(); }

  void stop() {
    Proxy::stopDma();
    resetRxState();
    state_ = TransportState_e::STOPPED;
  }

  RxSpan_s consumeRx() {
    if (state_ != TransportState_e::READY) {
      return {};
    }

    uint32_t producedBefore = 0;
    if (!snapshotProducer(producedBefore)) {
      rxErrorPending_ = true;
      return RxSpan_s{.overwritten = true};
    }

    if (producedBefore == rxConsumed_) {
      return {};
    }

    const uint32_t backlogBefore = producedBefore - rxConsumed_;
    if (backlogBefore > RX_BUFFER_SIZE) {
      // With no CRC there is no reliable way to find a retained frame start
      // inside a lapped ring. Drop this backlog and force a clean RX restart.
      rxErrorPending_ = true;
      return RxSpan_s{.overwritten = true};
    }

    // Parser input is always a short scratch copy, never the DMA-owned ring.
    const size_t readPosition =
        static_cast<size_t>(rxConsumed_ % RX_BUFFER_SIZE);
    const size_t copySize =
        std::min({static_cast<size_t>(backlogBefore), RX_SCRATCH_SIZE,
                  RX_BUFFER_SIZE - readPosition});
    std::memcpy(rxScratch_.data(), rxBuffer_.data() + readPosition, copySize);

    uint32_t producedAfter = 0;
    if (!snapshotProducer(producedAfter)) {
      rxErrorPending_ = true;
      return RxSpan_s{.overwritten = true};
    }

    // The second snapshot detects a full-ring lap while memcpy() was running.
    // Equality is safe: at exactly one ring of advance DMA has not yet written
    // the source byte at the next position.
    if (producedAfter - rxConsumed_ > RX_BUFFER_SIZE) {
      rxErrorPending_ = true;
      return RxSpan_s{.overwritten = true};
    }

    rxConsumed_ += static_cast<uint32_t>(copySize);
    return RxSpan_s{
        .bytes = std::span<const std::byte>(rxScratch_.data(), copySize),
    };
  }

  APP::WGS::PROTOCOL::Result_s tryWrite(std::span<const std::byte> _bytes) {
    if (state_ != TransportState_e::READY) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    if (_bytes.empty() || _bytes.size() > UINT16_MAX) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    auto *data =
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(_bytes.data()));
    if (Proxy::transmitBlocking(data, _bytes.size(), TX_TIMEOUT_MS) != HAL_OK) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::TRANSPORT_BUSY);
    }

    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void onRxEventFromIsr(uint16_t _position) {
    if (_position > RX_BUFFER_SIZE) {
      return;
    }

    const uint16_t previous = isrLastPosition_;
    const uint32_t delta = _position >= previous
                               ? static_cast<uint32_t>(_position - previous)
                               : static_cast<uint32_t>(RX_BUFFER_SIZE - previous +
                                                       _position);
    if (delta == 0U) {
      return;
    }

    rxEventProduced_ = rxEventProduced_ + delta;
    isrLastPosition_ = _position;
  }

  void onErrorFromIsr() { rxErrorPending_ = true; }

  bool hasPendingError() const { return rxErrorPending_; }

  APP::WGS::PROTOCOL::Result_s restart() {
    Proxy::stopDma();
    resetRxState();

    auto *buffer = reinterpret_cast<uint8_t *>(rxBuffer_.data());
    if (Proxy::startRxDma(buffer, rxBuffer_.size()) != HAL_OK) {
      state_ = TransportState_e::FAULT;
      rxErrorPending_ = true;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    state_ = TransportState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  TransportState_e state() const { return state_; }

private:
  bool snapshotProducer(uint32_t &_produced) const {
    // DMA keeps running; this only freezes the ISR-maintained event baseline
    // for the three reads below. HT remains enabled, so the ISR must run at
    // least once per half-ring: about 44.4 ms at 921600 baud. Missing that
    // deadline makes delayed HT/TC indistinguishable from repeated IDLE and
    // NDTR alone cannot count whole revolutions.
    const uint32_t primask = Proxy::irqSave();
    const uint32_t eventProduced = rxEventProduced_;
    const uint16_t rawEventPosition = isrLastPosition_;
    const size_t dmaRemaining = Proxy::rxDmaRemaining();
    Proxy::irqRestore(primask);

    if (dmaRemaining > RX_BUFFER_SIZE) {
      return false;
    }

    const size_t eventPosition =
        rawEventPosition == RX_BUFFER_SIZE ? 0U : rawEventPosition;
    const size_t dmaPosition = RX_BUFFER_SIZE - dmaRemaining;
    const size_t inFlight =
        (dmaPosition + RX_BUFFER_SIZE - eventPosition) % RX_BUFFER_SIZE;
    if (inFlight > RX_BUFFER_SIZE / 2U) {
      return false;
    }

    _produced = eventProduced + static_cast<uint32_t>(inFlight);
    return true;
  }

  void resetRxState() {
    isrLastPosition_ = 0;
    rxEventProduced_ = 0;
    rxConsumed_ = 0;
    rxErrorPending_ = false;
  }

  std::array<std::byte, RX_BUFFER_SIZE> rxBuffer_{};
  std::array<std::byte, RX_SCRATCH_SIZE> rxScratch_{};
  volatile uint16_t isrLastPosition_{0};
  volatile uint32_t rxEventProduced_{0};
  uint32_t rxConsumed_{0};
  volatile bool rxErrorPending_{false};
  TransportState_e state_{TransportState_e::STOPPED};
};

} // namespace APP::WGS::COMM
