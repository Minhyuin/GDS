#pragma once

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

struct PingCommand_s {};

struct DacCodePoint_s {
  uint16_t x{0};
  uint16_t y{0};
};

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
