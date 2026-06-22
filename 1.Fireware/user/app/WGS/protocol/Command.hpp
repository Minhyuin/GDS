#pragma once

#include "DacTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace APP::WGS::PROTOCOL {

enum class CommandType_e : uint8_t {
  PING = 0x00,
  SYNC = 0x01,
  POINT_SET = 0x10,
  SEQUENCE_SET = 0x20,
  WAVEFORM_CONFIG = 0x30,
  TIMER_SET = 0x40,
};

enum class Channel_e : uint8_t {
  CH1 = 0x01,
  CH2 = 0x02,
};

inline constexpr size_t PERIOD_SAMPLE_COUNT = 1000;
inline constexpr size_t MAX_SEQUENCE_POINTS = PERIOD_SAMPLE_COUNT;
inline constexpr size_t POINT_SET_PAYLOAD_SIZE = 10;
inline constexpr size_t TIMER_SET_PAYLOAD_SIZE = 6;
inline constexpr size_t WAVEFORM_CONFIG_PAYLOAD_SIZE = 8;
inline constexpr size_t SEQUENCE_SET_HEADER_SIZE = 4;
inline constexpr size_t SEQUENCE_POINT_SIZE = sizeof(float);
inline constexpr size_t SEQUENCE_SET_MAX_PAYLOAD_SIZE =
    SEQUENCE_SET_HEADER_SIZE + MAX_SEQUENCE_POINTS * SEQUENCE_POINT_SIZE;

struct PingCommand_s {};
struct SyncCommand_s {};

using DacSample_s = APP::WGS::OUTPUT::DacSample_s;

struct PointSetCommand_s {
  Channel_e channel{Channel_e::CH1};
  DacSample_s sample{};
  float ackPulseUs{0.0F};
};

struct SequenceSetCommand_s {
  Channel_e channel{Channel_e::CH1};
  std::span<const DacSample_s> samples{};
};

struct WaveformConfigCommand_s {
  Channel_e channel{Channel_e::CH1};
  uint16_t lengthSa{PERIOD_SAMPLE_COUNT};
  float frequencyHz{0.0F};
};

struct TimerSetCommand_s {
  Channel_e channel{Channel_e::CH1};
  float sampleRateHz{0.0F};
};

struct GetStatusCommand_s {};

struct EmergencyStopCommand_s {};

using Command_v =
    std::variant<PingCommand_s, SyncCommand_s, PointSetCommand_s,
                 SequenceSetCommand_s, WaveformConfigCommand_s,
                 TimerSetCommand_s, GetStatusCommand_s, EmergencyStopCommand_s>;

} // namespace APP::WGS::PROTOCOL
