#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace APP::WGS::PROTOCOL {

struct Crc16Ccitt_s {
  static constexpr uint16_t INITIAL = 0xFFFF;
  static constexpr uint16_t POLY = 0x1021;

  static constexpr uint16_t update(uint16_t _crc, uint8_t _byte) {
    _crc ^= static_cast<uint16_t>(_byte) << 8;
    for (uint8_t i = 0; i < 8; ++i) {
      if ((_crc & 0x8000U) != 0U) {
        _crc = static_cast<uint16_t>((_crc << 1U) ^ POLY);
      } else {
        _crc = static_cast<uint16_t>(_crc << 1U);
      }
    }
    return _crc;
  }

  static constexpr uint16_t compute(std::span<const std::byte> _bytes) {
    uint16_t crc = INITIAL;
    for (auto byte : _bytes) {
      crc = update(crc, static_cast<uint8_t>(byte));
    }
    return crc;
  }
};

} // namespace APP::WGS::PROTOCOL
