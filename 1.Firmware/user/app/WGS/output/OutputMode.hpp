#pragma once

#include "Command.hpp"
#include "WaveformStore.hpp"

namespace APP::WGS::OUTPUT {

inline OutputChannel_e
toOutputChannel(APP::WGS::PROTOCOL::Channel_e _channel) {
  return _channel == APP::WGS::PROTOCOL::Channel_e::CH1
             ? OutputChannel_e::CH1
             : OutputChannel_e::CH2;
}

template <typename Player_s> struct OutputContext_s {
  Player_s &player;
  WaveformStore &store;
};

template <typename Player_s> class BufferedOutputMode {
public:
  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::SyncCommand_s &) {
    return _ctx.player.sync(_ctx.store);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointSetCommand_s &_cmd) {
    return _ctx.player.holdChannel(
        toOutputChannel(_cmd.channel), _cmd.sample.value,
        _ctx.player.pointPulseWidthUs());
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::Point2SetCommand_s &_cmd) {
    return _ctx.player.holdChannels(
        _cmd.ch1Sample.value, _cmd.ch2Sample.value,
        _ctx.player.pointPulseWidthUs());
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointFifoClearCommand_s &_cmd) {
    return _ctx.player.clearPointFifo(toOutputChannel(_cmd.channel));
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointFifoAppendCommand_s &_cmd) {
    return _ctx.player.appendPointFifo(toOutputChannel(_cmd.channel),
                                       _cmd.points);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointFifoStartCommand_s &_cmd) {
    return _ctx.player.startPointFifo(toOutputChannel(_cmd.channel));
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointFifoStopCommand_s &_cmd) {
    return _ctx.player.stopPointFifo(toOutputChannel(_cmd.channel));
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::Point2FifoClearCommand_s &) {
    return _ctx.player.clearPoint2Fifo();
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::Point2FifoAppendCommand_s &_cmd) {
    return _ctx.player.appendPoint2Fifo(_cmd.points);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::Point2FifoStartCommand_s &) {
    return _ctx.player.startPoint2Fifo();
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::Point2FifoStopCommand_s &) {
    return _ctx.player.stopPoint2Fifo();
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::PointPulseWidthSetCommand_s &_cmd) {
    return _ctx.player.setPointPulseWidth(_cmd.pulseWidthUs);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::SequenceSetCommand_s &_cmd) {
    return _ctx.player.replaceSequence(
        toOutputChannel(_cmd.channel), _cmd.samples, _ctx.store);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::WaveformConfigCommand_s &_cmd) {
    return _ctx.player.configureWaveform(
        toOutputChannel(_cmd.channel), _cmd.lengthSa, _cmd.frequencyHz,
        _ctx.store);
  }

  APP::WGS::PROTOCOL::Result_s
  handle(OutputContext_s<Player_s> &_ctx,
         const APP::WGS::PROTOCOL::TimerSetCommand_s &_cmd) {
    return _ctx.player.setSampleRate(toOutputChannel(_cmd.channel),
                                     _cmd.sampleRateHz);
  }

  template <typename Command_s>
  APP::WGS::PROTOCOL::Result_s handle(OutputContext_s<Player_s> &,
                                      const Command_s &) {
    return APP::WGS::PROTOCOL::Result_s::fail(
        APP::WGS::PROTOCOL::ErrorCode_e::UNSUPPORTED_COMMAND);
  }
};

} // namespace APP::WGS::OUTPUT
