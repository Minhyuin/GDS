#pragma once

#include "Command.hpp"
#include "WaveformStore.hpp"

#include <cmath>
#include <variant>

namespace APP::WGS::OUTPUT {

inline constexpr float DAC_REFERENCE_VOLTAGE = 3.3F;
inline constexpr uint32_t DAC_FULL_SCALE_CODE = 4095;

inline float clampOutputVoltage(float _voltage) {
  if (!std::isfinite(_voltage) || _voltage <= 0.0F) {
    return 0.0F;
  }
  if (_voltage >= DAC_REFERENCE_VOLTAGE) {
    return DAC_REFERENCE_VOLTAGE;
  }
  return _voltage;
}

inline uint16_t voltageToDacCode(float _voltage) {
  const float clamped = clampOutputVoltage(_voltage);
  const float scaled =
      std::round((clamped * static_cast<float>(DAC_FULL_SCALE_CODE)) /
                 DAC_REFERENCE_VOLTAGE);
  if (scaled <= 0.0F) {
    return 0;
  }
  if (scaled >= static_cast<float>(DAC_FULL_SCALE_CODE)) {
    return static_cast<uint16_t>(DAC_FULL_SCALE_CODE);
  }
  return static_cast<uint16_t>(scaled);
}

enum class OutputState_e : uint8_t {
  IDLE,
  BUFFERING,
  FAULT,
};

template <typename Player_s> struct OutputContext_s {
  Player_s *player{nullptr};
  WaveformStore *store{nullptr};
  OutputState_e state{OutputState_e::IDLE};
};

template <typename Player_s> class BufferedOutputMode {
public:
  APP::WGS::PROTOCOL::Result_s enter(OutputContext_s<Player_s> &_ctx) {
    _ctx.state = OutputState_e::BUFFERING;
    return APP::WGS::PROTOCOL::Result_s::success();
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointSetCommand_s &_cmd) {
    if (_ctx.store == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    const DacFrame_s frame{
        .x = voltageToDacCode(_cmd.xVoltage),
        .y = voltageToDacCode(_cmd.yVoltage),
    };
    auto result = _ctx.store->push(std::span<const DacFrame_s>(&frame, 1));
    if (result.ok()) {
      _ctx.state = OutputState_e::BUFFERING;
    }
    return result;
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::SequenceSetCommand_s &_cmd) {
    if (_ctx.store == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    if (_cmd.points.empty()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    if (_cmd.points.size() > _ctx.store->writable()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BUFFER_FULL);
    }

    auto result = _ctx.store->push(_cmd.points);
    if (result.ok()) {
      _ctx.state = OutputState_e::BUFFERING;
    }
    return result;
  }

  template <typename Command_s>
  APP::WGS::PROTOCOL::Result_s handle(OutputContext_s<Player_s> &,
                                      const Command_s &) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::UNSUPPORTED_COMMAND);
  }
};

template <typename Player_s>
using OutputMode_v = std::variant<BufferedOutputMode<Player_s>>;

} // namespace APP::WGS::OUTPUT
