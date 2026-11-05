#pragma once

#include "OutputMode.hpp"

#include <variant>

namespace APP::WGS::OUTPUT {

template <typename Player> class OutputController {
public:
  OutputController(Player &_player, WaveformStore &_store)
      : context_{_player, _store} {}

  APP::WGS::PROTOCOL::Result_s start() {
    return context_.player.start(context_.store);
  }

  APP::WGS::PROTOCOL::Result_s
  dispatch(const APP::WGS::PROTOCOL::Command_v &_command) {
    return std::visit(
        [&](const auto &_cmd) { return mode_.handle(context_, _cmd); },
        _command);
  }

  void firePendingPointPulseFromIsr() {
    context_.player.firePendingPointPulseFromIsr();
  }

  void completePointPulseFromIsr(OutputChannel_e _channel) {
    context_.player.completePointPulseFromIsr(_channel);
  }

private:
  OutputContext_s<Player> context_;
  BufferedOutputMode<Player> mode_{};
};

} // namespace APP::WGS::OUTPUT
