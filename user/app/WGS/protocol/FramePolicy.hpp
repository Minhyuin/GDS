#pragma once

#include "Command.hpp"
#include "Crc.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::PROTOCOL {

enum class FrameType_e : uint8_t {
  COMMAND = 0x01,
  ACK = 0x02,
  NACK = 0x03,
  EVENT = 0x04,
  STATUS = 0x05,
};

struct FrameHeader_s {
  FrameType_e type{FrameType_e::COMMAND};
  uint16_t length{0};
};

struct Frame_s {
  FrameHeader_s header{};
  std::span<const std::byte> payload{};
};

struct GdsFrameV1Policy_s {
  static constexpr uint8_t SOF = 0xA5;
  static constexpr bool CRC_ENABLED = false;
  static constexpr size_t HEADER_SIZE = 3;
  static constexpr size_t CRC_SIZE = CRC_ENABLED ? 2 : 0;
  static constexpr size_t MAX_PAYLOAD = SEQUENCE_SET_MAX_PAYLOAD_SIZE;

  using Crc_s = Crc16Ccitt_s;

  static constexpr uint16_t readU16(std::span<const std::byte> _bytes,
                                    size_t _offset) {
    return static_cast<uint16_t>(
        static_cast<uint8_t>(_bytes[_offset]) |
        (static_cast<uint16_t>(static_cast<uint8_t>(_bytes[_offset + 1]))
         << 8U));
  }

  static constexpr FrameHeader_s
  parseHeader(std::span<const std::byte> _bytes) {
    return FrameHeader_s{
        .type = static_cast<FrameType_e>(static_cast<uint8_t>(_bytes[0])),
        .length = readU16(_bytes, 1),
    };
  }
};

static_assert(GdsFrameV1Policy_s::MAX_PAYLOAD == 4004);
static_assert(GdsFrameV1Policy_s::MAX_PAYLOAD <= 4096);

} // namespace APP::WGS::PROTOCOL
