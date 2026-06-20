#pragma once

#include "Result.hpp"
#include "TransportTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::COMM {

enum class TransportState_e : uint8_t {
  STOPPED,
  READY,
  TX_BUSY,
  FAULT,
};

template <typename Proxy> class UsartDmaTransport {
public:
  static constexpr size_t RX_BUFFER_SIZE = 256;

  UsartDmaTransport() = default;
  ~UsartDmaTransport() { stop(); }

  APP::WGS::PROTOCOL::Result_s start() {
    auto *buffer = reinterpret_cast<uint8_t *>(rxBuffer_.data());
    if (Proxy::startRxDma(buffer, rxBuffer_.size()) != HAL_OK) {
      state_ = TransportState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    lastRxPos_ = 0;
    state_ = TransportState_e::READY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void stop() {
    Proxy::stopDma();
    state_ = TransportState_e::STOPPED;
  }

  RxSpan_s consumeRx() {
    if (state_ == TransportState_e::STOPPED) {
      return {};
    }

    const size_t dmaRemaining = Proxy::rxDmaRemaining();
    const size_t currentPos = RX_BUFFER_SIZE - dmaRemaining;
    if (currentPos == lastRxPos_) {
      return {};
    }

    if (currentPos > lastRxPos_) {
      auto span = std::span<const std::byte>(rxBuffer_.data() + lastRxPos_,
                                             currentPos - lastRxPos_);
      lastRxPos_ = currentPos;
      return RxSpan_s{.bytes = span};
    }

    auto span = std::span<const std::byte>(rxBuffer_.data() + lastRxPos_,
                                           RX_BUFFER_SIZE - lastRxPos_);
    lastRxPos_ = 0;
    return RxSpan_s{.bytes = span};
  }

  APP::WGS::PROTOCOL::Result_s tryWrite(std::span<const std::byte> _bytes) {
    if (state_ == TransportState_e::TX_BUSY) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::TRANSPORT_BUSY);
    }

    if (_bytes.empty()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    auto *data =
        const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(_bytes.data()));
    if (Proxy::transmitDma(data, _bytes.size()) != HAL_OK) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::TRANSPORT_BUSY);
    }

    state_ = TransportState_e::TX_BUSY;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  void onTxComplete() {
    if (state_ == TransportState_e::TX_BUSY) {
      state_ = TransportState_e::READY;
    }
  }

  TransportState_e state() const { return state_; }

private:
  std::array<std::byte, RX_BUFFER_SIZE> rxBuffer_{};
  size_t lastRxPos_{0};
  TransportState_e state_{TransportState_e::STOPPED};
};

} // namespace APP::WGS::COMM
