#pragma once

#include "Reply.hpp"
#include "Result.hpp"

#include <array>
#include <cstddef>
#include <span>

namespace APP::WGS::COMM {

template <typename Transport, typename Parser, typename FramePolicy>
class ProtocolService {
public:
  ProtocolService(Transport &_transport, Parser &_parser)
      : transport_(&_transport), parser_(&_parser) {}

  APP::WGS::PROTOCOL::Result_s
  sendAck(const APP::WGS::PROTOCOL::AckPayload_s &_ack) {
    return sendReply(APP::WGS::PROTOCOL::FrameType_e::ACK, _ack);
  }

  APP::WGS::PROTOCOL::Result_s
  sendNack(APP::WGS::PROTOCOL::ErrorCode_e _error) {
    return sendReply(APP::WGS::PROTOCOL::FrameType_e::NACK,
                     APP::WGS::PROTOCOL::AckPayload_s{
                         .status = APP::WGS::PROTOCOL::AckStatus_e::ERROR,
                         .detail = _error,
                     });
  }

private:
  static constexpr size_t SOF_SIZE = 1;
  static constexpr size_t REPLY_PAYLOAD_SIZE = 2;
  static constexpr size_t REPLY_FRAME_SIZE =
      SOF_SIZE + FramePolicy::HEADER_SIZE + REPLY_PAYLOAD_SIZE +
      FramePolicy::CRC_SIZE;

  APP::WGS::PROTOCOL::Result_s
  sendReply(APP::WGS::PROTOCOL::FrameType_e _type,
            const APP::WGS::PROTOCOL::AckPayload_s &_ack) {
    txFrame_[0] = static_cast<std::byte>(FramePolicy::SOF);
    txFrame_[1] = static_cast<std::byte>(_type);
    txFrame_[2] = static_cast<std::byte>(REPLY_PAYLOAD_SIZE & 0xFFU);
    txFrame_[3] = static_cast<std::byte>((REPLY_PAYLOAD_SIZE >> 8U) & 0xFFU);
    txFrame_[4] = static_cast<std::byte>(_ack.status);
    txFrame_[5] = static_cast<std::byte>(_ack.detail);

    if constexpr (FramePolicy::CRC_ENABLED) {
      const auto crc = FramePolicy::Crc_s::compute(std::span<const std::byte>(
          txFrame_.data() + SOF_SIZE,
          FramePolicy::HEADER_SIZE + REPLY_PAYLOAD_SIZE));
      txFrame_[6] = static_cast<std::byte>(crc & 0xFFU);
      txFrame_[7] = static_cast<std::byte>((crc >> 8U) & 0xFFU);
    }

    return transport_->tryWrite(
        std::span<const std::byte>(txFrame_.data(), txFrame_.size()));
  }

  Transport *transport_{nullptr};
  Parser *parser_{nullptr};
  std::array<std::byte, REPLY_FRAME_SIZE> txFrame_{};
};

} // namespace APP::WGS::COMM
