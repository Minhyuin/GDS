#pragma once

#include "DacProxy.hpp"
#include "DacWavePlayer.hpp"
#include "FramePolicy.hpp"
#include "OutputController.hpp"
#include "ProtocolParser.hpp"
#include "ProtocolService.hpp"
#include "TimProxy.hpp"
#include "UsartDmaTransport.hpp"
#include "UsartProxy.hpp"
#include "WaveformStore.hpp"

#include "FreeRTOS.h"
#include "task.h"

#include <array>

namespace APP::WGS {

class WGSManager {
public:
  WGSManager();

  PROTOCOL::Result_s init();
  void notifyRxFromIsr();
  void onTxCompleteFromIsr();
  void onDacDmaHalfCompleteFromIsr();
  void onDacDmaCompleteFromIsr();

private:
  static constexpr uint32_t TASK_STACK_WORDS = 256;
  static constexpr UBaseType_t TASK_PRIORITY = tskIDLE_PRIORITY + 3;
  static constexpr size_t MAX_SEQUENCE_POINTS = PROTOCOL::MAX_SEQUENCE_POINTS;

  using Usart1 = BSP::UsartProxy<BSP::Usart1HandleProvider>;
  using Tim6 = BSP::TimProxy<BSP::Tim6HandleProvider>;

  using Parser = PROTOCOL::ProtocolParser<PROTOCOL::GdsFrameV1Policy_s>;

  using Transport = COMM::UsartDmaTransport<Usart1>;
  using DacPlayer = OUTPUT::DacWavePlayer<BSP::DacProxy, Tim6>;

  using ProtocolService =
      COMM::ProtocolService<Transport, Parser, PROTOCOL::GdsFrameV1Policy_s>;
  using OutputController = OUTPUT::OutputController<DacPlayer>;

  static void taskEntry(void *_argument);

  PROTOCOL::Result_s startTask();
  void taskLoop();
  void processRx();
  void handleFrame(const PROTOCOL::Frame_s &_frame);
  PROTOCOL::Result_s decodeCommand(const PROTOCOL::Frame_s &_frame,
                                   PROTOCOL::Command_v &_command);
  void sendResult(PROTOCOL::Result_s _result);

  Transport transport_;
  Parser parser_;
  ProtocolService protocol_;
  OUTPUT::WaveformStore waveformStore_;
  DacPlayer dacPlayer_;
  OutputController output_;
  std::array<PROTOCOL::DacCodePoint_s, MAX_SEQUENCE_POINTS> decodeScratch_{};
  StaticTask_t taskControlBlock_{};
  StackType_t taskStack_[TASK_STACK_WORDS]{};
  TaskHandle_t taskHandle_{nullptr};
};

} // namespace APP::WGS

extern "C" void notifyWgsRxFromIsr();
extern "C" void notifyWgsTxCompleteFromIsr();
extern "C" void notifyWgsDacDmaHalfCompleteFromIsr();
extern "C" void notifyWgsDacDmaCompleteFromIsr();
