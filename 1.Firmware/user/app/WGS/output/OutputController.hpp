#pragma once

#include "OutputMode.hpp"

namespace APP::WGS::OUTPUT {

template <typename Player> class OutputController {
public:
  OutputController(Player &_player, WaveformStore &_store) {
    context_.player = &_player;
    context_.store = &_store;
    enterBufferedMode();
  }

  APP::WGS::PROTOCOL::Result_s enterBufferedMode() {
    mode_.template emplace<BufferedOutputMode<Player>>();
    return std::get<BufferedOutputMode<Player>>(mode_).enter(context_);
  }

  APP::WGS::PROTOCOL::Result_s start() {
    if (context_.player == nullptr) {
      context_.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }
    if (context_.store == nullptr) {
      context_.state = OutputState_e::FAULT;
      return APP::WGS::PROTOCOL::Result_s::fail(
          APP::WGS::PROTOCOL::ErrorCode_e::INVALID_STATE);
    }
    return context_.player->start(*context_.store);
  }

  APP::WGS::PROTOCOL::Result_s
  dispatch(const APP::WGS::PROTOCOL::Command_v &_command) {
    return std::visit(
        [&](const auto &_cmd) {
          return std::visit(
              [&](auto &_mode) { return _mode.handle(context_, _cmd); }, mode_);
        },
        _command);
  }

  void firePendingPointPulseFromIsr() {
    if (context_.player != nullptr) {
      context_.player->firePendingPointPulseFromIsr();
    }
  }

  void completePointPulseFromIsr(OutputChannel_e _channel) {
    if (context_.player != nullptr) {
      context_.player->completePointPulseFromIsr(_channel);
    }
  }

  OutputState_e state() const { return context_.state; }

private:
  OutputContext_s<Player> context_{};
  OutputMode_v<Player> mode_{BufferedOutputMode<Player>{}};
};

} // namespace APP::WGS::OUTPUT
