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

inline OutputChannel_e
toOutputChannel(APP::WGS::PROTOCOL::Channel_e _channel) {
  return _channel == APP::WGS::PROTOCOL::Channel_e::CH1
             ? OutputChannel_e::CH1
             : OutputChannel_e::CH2;
}

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
         const APP::WGS::PROTOCOL::SyncCommand_s &) {
    if (_ctx.store == nullptr || _ctx.player == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    auto result = _ctx.player->sync(*_ctx.store);
    if (result.ok()) {
      _ctx.state = OutputState_e::BUFFERING;
    }
    return result;
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointSetCommand_s &_cmd) {
    if (_ctx.player == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    const auto channel = toOutputChannel(_cmd.channel);
    auto result = _ctx.player->holdChannel(channel, _cmd.sample.value);
    if (result.ok()) {
      _ctx.state = OutputState_e::BUFFERING;
    }
    return result;
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::SequenceSetCommand_s &_cmd) {
    if (_ctx.store == nullptr || _ctx.player == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    if (_cmd.samples.empty()) {
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const auto channel = toOutputChannel(_cmd.channel);
    auto result = _ctx.store->setSequence(channel, _cmd.samples);
    if (!result.ok()) {
      return result;
    }

    result = _ctx.player->reloadChannel(channel, *_ctx.store);
    if (result.ok()) {
      _ctx.state = OutputState_e::BUFFERING;
    }
    return result;
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::TimerSetCommand_s &_cmd) {
    if (_ctx.player == nullptr) {
      _ctx.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }

    return _ctx.player->setSampleRate(toOutputChannel(_cmd.channel),
                                      _cmd.sampleRateHz);
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
