#pragma once

#include "FramePolicy.hpp"
#include "Result.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace APP::WGS::PROTOCOL {

enum class ParserState_e : uint8_t {
  SEEK_SOF,
  HEADER,
  PAYLOAD,
  CRC_LOW,
  CRC_HIGH,
};

template <typename Policy> class ProtocolParser {
public:
  struct ParseResult_s {
    Result_s result{};
    std::optional<Frame_s> frame{};
  };

  constexpr ProtocolParser() = default;

  ParseResult_s feed(uint8_t _byte) {
    switch (state_) {
    case ParserState_e::SEEK_SOF:
      return seekSof(_byte);
    case ParserState_e::HEADER:
      return readHeader(_byte);
    case ParserState_e::PAYLOAD:
      return readPayload(_byte);
    case ParserState_e::CRC_LOW:
      crcLow_ = _byte;
      state_ = ParserState_e::CRC_HIGH;
      return {};
    case ParserState_e::CRC_HIGH:
      return finishFrame(_byte);
    }

    reset();
    return {.result = Result_s::fail(ErrorCode_e::INVALID_STATE)};
  }

  void reset() {
    state_ = ParserState_e::SEEK_SOF;
    headerIndex_ = 0;
    payloadIndex_ = 0;
    payloadLength_ = 0;
    crcLow_ = 0;
  }

private:
  ParseResult_s seekSof(uint8_t _byte) {
    if (_byte == Policy::SOF) {
      state_ = ParserState_e::HEADER;
      headerIndex_ = 0;
    }
    return {};
  }

  ParseResult_s readHeader(uint8_t _byte) {
    headerBuffer_[headerIndex_++] = static_cast<std::byte>(_byte);
    if (headerIndex_ < Policy::HEADER_SIZE) {
      return {};
    }

    currentHeader_ = Policy::parseHeader(
        std::span<const std::byte>(headerBuffer_.data(), Policy::HEADER_SIZE));
    payloadLength_ = currentHeader_.length;
    if (payloadLength_ > Policy::MAX_PAYLOAD) {
      reset();
      return {.result = Result_s::fail(ErrorCode_e::BAD_LENGTH)};
    }

    payloadIndex_ = 0;
    if (payloadLength_ == 0) {
      return finishFrameWithoutCrc();
    }

    state_ = ParserState_e::PAYLOAD;
    return {};
  }

  ParseResult_s readPayload(uint8_t _byte) {
    payloadBuffer_[payloadIndex_++] = static_cast<std::byte>(_byte);
    if (payloadIndex_ >= payloadLength_) {
      if constexpr (!Policy::CRC_ENABLED) {
        return finishFrameWithoutCrc();
      }
      state_ = ParserState_e::CRC_LOW;
    }
    return {};
  }

  ParseResult_s finishFrame(uint8_t _crcHigh) {
    const uint16_t receivedCrc = static_cast<uint16_t>(
        crcLow_ | (static_cast<uint16_t>(_crcHigh) << 8U));
    const uint16_t computedCrc = computeFrameCrc();
    if (receivedCrc != computedCrc) {
      reset();
      return {.result = Result_s::fail(ErrorCode_e::CRC_ERROR)};
    }

    auto payload =
        std::span<const std::byte>(payloadBuffer_.data(), payloadLength_);
    Frame_s frame{.header = currentHeader_, .payload = payload};
    reset();
    return {.result = Result_s::success(), .frame = frame};
  }

  uint16_t computeFrameCrc() const {
    uint16_t crc = Policy::Crc_s::INITIAL;
    for (size_t i = 0; i < Policy::HEADER_SIZE; ++i) {
      crc = Policy::Crc_s::update(crc, static_cast<uint8_t>(headerBuffer_[i]));
    }
    for (size_t i = 0; i < payloadLength_; ++i) {
      crc = Policy::Crc_s::update(crc, static_cast<uint8_t>(payloadBuffer_[i]));
    }
    return crc;
  }

  ParseResult_s finishFrameWithoutCrc() {
    auto payload =
        std::span<const std::byte>(payloadBuffer_.data(), payloadLength_);
    Frame_s frame{.header = currentHeader_, .payload = payload};
    reset();
    return {.result = Result_s::success(), .frame = frame};
  }

  ParserState_e state_{ParserState_e::SEEK_SOF};
  std::array<std::byte, Policy::HEADER_SIZE> headerBuffer_{};
  std::array<std::byte, Policy::MAX_PAYLOAD> payloadBuffer_{};
  FrameHeader_s currentHeader_{};
  size_t headerIndex_{0};
  size_t payloadIndex_{0};
  size_t payloadLength_{0};
  uint8_t crcLow_{0};
};

} // namespace APP::WGS::PROTOCOL
