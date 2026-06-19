#pragma once

#include "DacTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace APP::WGS::PROTOCOL {

enum class CommandType_e : uint8_t {
  PING = 0x00,
  POINT_SET = 0x10,
  SEQUENCE_SET = 0x20,
  GET_STATUS = 0x30,     // TODO:
  EMERGENCY_STOP = 0x7F, // TODO:
};

inline constexpr size_t MAX_SEQUENCE_POINTS = 255;
inline constexpr size_t POINT_SET_PAYLOAD_SIZE = 9;
inline constexpr size_t SEQUENCE_SET_HEADER_SIZE = 3;
inline constexpr size_t SEQUENCE_POINT_SIZE = 8;
inline constexpr size_t SEQUENCE_SET_MAX_PAYLOAD_SIZE =
    SEQUENCE_SET_HEADER_SIZE + MAX_SEQUENCE_POINTS * SEQUENCE_POINT_SIZE;

struct PingCommand_s {};

using DacCodePoint_s = APP::WGS::OUTPUT::DacFrame_s;

struct PointSetCommand_s {
  float xVoltage{0.0F};
  float yVoltage{0.0F};
};

struct SequenceSetCommand_s {
  std::span<const DacCodePoint_s> points{};
};

struct GetStatusCommand_s {};

struct EmergencyStopCommand_s {};

using Command_v = std::variant<PingCommand_s, PointSetCommand_s,
                               SequenceSetCommand_s, GetStatusCommand_s,
                               EmergencyStopCommand_s>;

} // namespace APP::WGS::PROTOCOL
