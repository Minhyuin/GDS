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
  POINT2_SET = 0x11,
  POINT_SET_NO_ACK = 0x12,
  POINT2_SET_NO_ACK = 0x13,
  POINT2_FIFO_CLEAR = 0x14,
  POINT2_FIFO_APPEND = 0x15,
  POINT2_FIFO_START = 0x16,
  POINT2_FIFO_STOP = 0x17,
  SEQUENCE_SET = 0x20,
  WAVEFORM_CONFIG = 0x30,
  TIMER_SET = 0x40,
  POINT_PULSE_WIDTH_SET = 0x50,
};

enum class Channel_e : uint8_t {
  CH1 = 0x01,
  CH2 = 0x02,
};

inline constexpr size_t PERIOD_SAMPLE_COUNT = 1000;
inline constexpr size_t MAX_SEQUENCE_POINTS = PERIOD_SAMPLE_COUNT;
inline constexpr size_t POINT_SET_PAYLOAD_SIZE = 6;
inline constexpr size_t POINT2_SET_PAYLOAD_SIZE = 9;
inline constexpr size_t POINT2_FIFO_CONTROL_PAYLOAD_SIZE = 1;
inline constexpr size_t POINT2_FIFO_APPEND_HEADER_SIZE = 3;
inline constexpr size_t POINT2_FIFO_POINT_SIZE = 8;
inline constexpr size_t POINT_PULSE_WIDTH_SET_PAYLOAD_SIZE = 5;
inline constexpr float POINT_PULSE_WIDTH_DEFAULT_US = 10.0F;
inline constexpr float POINT_PULSE_WIDTH_MIN_US = 1.0F;
inline constexpr float POINT_PULSE_WIDTH_MAX_US = 1000000.0F;
inline constexpr size_t TIMER_SET_PAYLOAD_SIZE = 6;
inline constexpr size_t WAVEFORM_CONFIG_PAYLOAD_SIZE = 8;
inline constexpr size_t SEQUENCE_SET_HEADER_SIZE = 4;
inline constexpr size_t SEQUENCE_POINT_SIZE = sizeof(float);
inline constexpr size_t SEQUENCE_SET_MAX_PAYLOAD_SIZE =
    SEQUENCE_SET_HEADER_SIZE + MAX_SEQUENCE_POINTS * SEQUENCE_POINT_SIZE;
inline constexpr size_t POINT2_FIFO_APPEND_MAX_POINTS = 500;

struct PingCommand_s {};
struct SyncCommand_s {};

using DacSample_s = APP::WGS::OUTPUT::DacSample_s;

struct PointSetCommand_s {
  Channel_e channel{Channel_e::CH1};
  DacSample_s sample{};
  bool replyEnabled{true};
};

struct Point2SetCommand_s {
  DacSample_s ch1Sample{};
  DacSample_s ch2Sample{};
  bool replyEnabled{true};
};

struct Point2FifoPoint_s {
  DacSample_s ch1Sample{};
  DacSample_s ch2Sample{};
};

struct Point2FifoClearCommand_s {};

struct Point2FifoAppendCommand_s {
  std::span<const Point2FifoPoint_s> points{};
};

struct Point2FifoStartCommand_s {};

struct Point2FifoStopCommand_s {};

struct PointPulseWidthSetCommand_s {
  float pulseWidthUs{10.0F};
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
                 Point2SetCommand_s, Point2FifoClearCommand_s,
                 Point2FifoAppendCommand_s, Point2FifoStartCommand_s,
                 Point2FifoStopCommand_s, PointPulseWidthSetCommand_s,
                 SequenceSetCommand_s, WaveformConfigCommand_s,
                 TimerSetCommand_s, GetStatusCommand_s, EmergencyStopCommand_s>;

} // namespace APP::WGS::PROTOCOL
