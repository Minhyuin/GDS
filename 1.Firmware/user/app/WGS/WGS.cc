#include "WGS.hpp"

#include "Buzzer.hpp"
#include "usart.h"

#include <cmath>
#include <cstring>

namespace APP::WGS {

namespace {
WGSManager *activeWgs = nullptr;
constexpr float MAX_OUTPUT_VOLTAGE = 3.3F;

uint32_t readU32(std::span<const std::byte> _bytes, size_t _offset) {
  return static_cast<uint32_t>(
      static_cast<uint8_t>(_bytes[_offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(_bytes[_offset + 1])) << 8U) |
      (static_cast<uint32_t>(static_cast<uint8_t>(_bytes[_offset + 2]))
       << 16U) |
      (static_cast<uint32_t>(static_cast<uint8_t>(_bytes[_offset + 3]))
       << 24U));
}

uint16_t readU16(std::span<const std::byte> _bytes, size_t _offset) {
  return static_cast<uint16_t>(
      static_cast<uint8_t>(_bytes[_offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(_bytes[_offset + 1]))
       << 8U));
}

float readFloat32Le(std::span<const std::byte> _bytes, size_t _offset) {
  const uint32_t raw = readU32(_bytes, _offset);
  float value = 0.0F;
  std::memcpy(&value, &raw, sizeof(value));
  return value;
}

float clampOutputVoltage(float _voltage) {
  if (!std::isfinite(_voltage) || _voltage <= 0.0F) {
    return 0.0F;
  }
  if (_voltage >= MAX_OUTPUT_VOLTAGE) {
    return MAX_OUTPUT_VOLTAGE;
  }
  return _voltage;
}

uint16_t voltageToDacCode(float _voltage) {
  constexpr float DAC_FULL_SCALE_CODE = 4095.0F;
  const float scaled =
      std::round((clampOutputVoltage(_voltage) * DAC_FULL_SCALE_CODE) /
                 MAX_OUTPUT_VOLTAGE);
  if (scaled <= 0.0F) {
    return 0;
  }
  if (scaled >= DAC_FULL_SCALE_CODE) {
    return static_cast<uint16_t>(DAC_FULL_SCALE_CODE);
  }
  return static_cast<uint16_t>(scaled);
}

bool readChannel(std::span<const std::byte> _bytes, size_t _offset,
                 PROTOCOL::Channel_e &_channel) {
  const auto raw = static_cast<uint8_t>(_bytes[_offset]);
  if (raw == static_cast<uint8_t>(PROTOCOL::Channel_e::CH1)) {
    _channel = PROTOCOL::Channel_e::CH1;
    return true;
  }
  if (raw == static_cast<uint8_t>(PROTOCOL::Channel_e::CH2)) {
    _channel = PROTOCOL::Channel_e::CH2;
    return true;
  }
  return false;
}
} // namespace

WGSManager::WGSManager()
    : protocol_(transport_, parser_), output_(dacPlayer_, waveformStore_) {
  activeWgs = this;
}

PROTOCOL::Result_s WGSManager::init() {
  auto result = transport_.start();
  if (!result.ok()) {
    return result;
  }

  result = output_.enterBufferedMode();
  if (!result.ok()) {
    return result;
  }

  result = output_.start();
  if (!result.ok()) {
    return result;
  }

  return startTask();
}

void WGSManager::notifyRxFromIsr() {
  if (taskHandle_ == nullptr) {
    return;
  }

  BaseType_t highPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(taskHandle_, &highPriorityTaskWoken);
  portYIELD_FROM_ISR(highPriorityTaskWoken);
}

void WGSManager::onTxCompleteFromIsr() { transport_.onTxComplete(); }

void WGSManager::onDacDmaHalfCompleteFromIsr() {}

void WGSManager::onDacDmaCompleteFromIsr() {}

PROTOCOL::Result_s WGSManager::startTask() {
  taskHandle_ =
      xTaskCreateStatic(taskEntry, "WGS_CMD", TASK_STACK_WORDS, this,
                        TASK_PRIORITY, taskStack_, &taskControlBlock_);

  if (taskHandle_ == nullptr)
    return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::INVALID_STATE);
  else
    return PROTOCOL::Result_s::success();
}

void WGSManager::taskEntry(void *_argument) {
  auto *self = static_cast<WGSManager *>(_argument);
  self->taskLoop();
}

void WGSManager::taskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    processRx();
  }
}

void WGSManager::processRx() {
  while (true) {
    auto rx = transport_.consumeRx();
    if (rx.bytes.empty()) {
      return;
    }

    for (auto byte : rx.bytes) {
      auto parsed = parser_.feed(static_cast<uint8_t>(byte));
      if (!parsed.result.ok()) {
        protocol_.sendNack(parsed.result.code);
        parser_.reset();
        continue;
      }

      if (parsed.frame) {
        handleFrame(*parsed.frame);
      }
    }
  }
}

void WGSManager::handleFrame(const PROTOCOL::Frame_s &_frame) {
  if (_frame.header.type != PROTOCOL::FrameType_e::COMMAND) {
    protocol_.sendNack(PROTOCOL::ErrorCode_e::UNSUPPORTED_COMMAND);
    return;
  }

  PROTOCOL::Command_v command;
  auto decodeResult = decodeCommand(_frame, command);
  if (!decodeResult.ok()) {
    protocol_.sendNack(decodeResult.code);
    return;
  }

  if (std::holds_alternative<PROTOCOL::PingCommand_s>(command)) {
    APP::BUZZER::notifyProtocolCommandReceived();
    sendResult(PROTOCOL::Result_s::success());
    return;
  }

  auto result = output_.dispatch(command);
  sendResult(result);
}

PROTOCOL::Result_s WGSManager::decodeCommand(const PROTOCOL::Frame_s &_frame,
                                             PROTOCOL::Command_v &_command) {
  if (_frame.payload.empty()) {
    return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
  }

  const auto commandType = static_cast<PROTOCOL::CommandType_e>(
      static_cast<uint8_t>(_frame.payload[0]));
  switch (commandType) {
  case PROTOCOL::CommandType_e::PING:
    if (_frame.payload.size() != 1) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    _command = PROTOCOL::PingCommand_s{};
    return PROTOCOL::Result_s::success();
  case PROTOCOL::CommandType_e::SYNC:
    if (_frame.payload.size() != 1) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    _command = PROTOCOL::SyncCommand_s{};
    return PROTOCOL::Result_s::success();
  case PROTOCOL::CommandType_e::POINT_SET: {
    if (_frame.payload.size() != PROTOCOL::POINT_SET_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    PROTOCOL::Channel_e channel{};
    if (!readChannel(_frame.payload, 1, channel)) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const float voltage = readFloat32Le(_frame.payload, 2);
    _command = PROTOCOL::PointSetCommand_s{
        .channel = channel,
        .sample = PROTOCOL::DacSample_s{.value = voltageToDacCode(voltage)},
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::POINT2_SET: {
    if (_frame.payload.size() != PROTOCOL::POINT2_SET_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const float ch1Voltage = readFloat32Le(_frame.payload, 1);
    const float ch2Voltage = readFloat32Le(_frame.payload, 5);

    _command = PROTOCOL::Point2SetCommand_s{
        .ch1Sample = PROTOCOL::DacSample_s{.value = voltageToDacCode(ch1Voltage)},
        .ch2Sample = PROTOCOL::DacSample_s{.value = voltageToDacCode(ch2Voltage)},
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::POINT_PULSE_WIDTH_SET: {
    if (_frame.payload.size() != PROTOCOL::POINT_PULSE_WIDTH_SET_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const float pulseWidthUs = readFloat32Le(_frame.payload, 1);
    if (!std::isfinite(pulseWidthUs) || pulseWidthUs < 1.0F ||
        pulseWidthUs > 1000000.0F) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    _command = PROTOCOL::PointPulseWidthSetCommand_s{
        .pulseWidthUs = pulseWidthUs,
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::SEQUENCE_SET: {
    if (_frame.payload.size() < PROTOCOL::SEQUENCE_SET_HEADER_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    PROTOCOL::Channel_e channel{};
    if (!readChannel(_frame.payload, 1, channel)) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const size_t pointCount = readU16(_frame.payload, 2);
    if (pointCount == 0U || pointCount > decodeScratch_.size()) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const size_t expectedPayloadSize =
        PROTOCOL::SEQUENCE_SET_HEADER_SIZE +
        pointCount * PROTOCOL::SEQUENCE_POINT_SIZE;
    if (_frame.payload.size() != expectedPayloadSize) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    size_t offset = PROTOCOL::SEQUENCE_SET_HEADER_SIZE;
    for (size_t i = 0; i < pointCount; ++i) {
      const float voltage = readFloat32Le(_frame.payload, offset);
      decodeScratch_[i] = PROTOCOL::DacSample_s{
          .value = voltageToDacCode(voltage),
      };
      offset += PROTOCOL::SEQUENCE_POINT_SIZE;
    }

    _command = PROTOCOL::SequenceSetCommand_s{
        .channel = channel,
        .samples = std::span<const PROTOCOL::DacSample_s>(
            decodeScratch_.data(), pointCount),
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::WAVEFORM_CONFIG: {
    if (_frame.payload.size() != PROTOCOL::WAVEFORM_CONFIG_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    PROTOCOL::Channel_e channel{};
    if (!readChannel(_frame.payload, 1, channel)) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const auto lengthSa = readU16(_frame.payload, 2);
    const float frequencyHz = readFloat32Le(_frame.payload, 4);

    _command = PROTOCOL::WaveformConfigCommand_s{
        .channel = channel,
        .lengthSa = lengthSa,
        .frequencyHz = frequencyHz,
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::TIMER_SET: {
    if (_frame.payload.size() != PROTOCOL::TIMER_SET_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    PROTOCOL::Channel_e channel{};
    if (!readChannel(_frame.payload, 1, channel)) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const float sampleRateHz = readFloat32Le(_frame.payload, 2);
    if (!std::isfinite(sampleRateHz)) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::OUT_OF_RANGE);
    }

    _command = PROTOCOL::TimerSetCommand_s{
        .channel = channel,
        .sampleRateHz = sampleRateHz,
    };
    return PROTOCOL::Result_s::success();
  }
  default:
    return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::UNSUPPORTED_COMMAND);
  }
}

void WGSManager::sendResult(PROTOCOL::Result_s _result) {
  if (_result.ok()) {
    protocol_.sendAck(PROTOCOL::AckPayload_s{
        .status = PROTOCOL::AckStatus_e::DONE,
        .detail = PROTOCOL::ErrorCode_e::NONE,
    });
    return;
  }

  protocol_.sendNack(_result.code);
}

} // namespace APP::WGS

extern "C" void notifyWgsRxFromIsr() {
  if (APP::WGS::activeWgs != nullptr) {
    APP::WGS::activeWgs->notifyRxFromIsr();
  }
}

extern "C" void notifyWgsTxCompleteFromIsr() {
  if (APP::WGS::activeWgs != nullptr) {
    APP::WGS::activeWgs->onTxCompleteFromIsr();
  }
}

extern "C" void notifyWgsDacDmaHalfCompleteFromIsr() {
  if (APP::WGS::activeWgs != nullptr) {
    APP::WGS::activeWgs->onDacDmaHalfCompleteFromIsr();
  }
}

extern "C" void notifyWgsDacDmaCompleteFromIsr() {
  if (APP::WGS::activeWgs != nullptr) {
    APP::WGS::activeWgs->onDacDmaCompleteFromIsr();
  }
}

extern "C" void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *) {
  notifyWgsDacDmaHalfCompleteFromIsr();
}

extern "C" void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *) {
  notifyWgsDacDmaCompleteFromIsr();
}

extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *_huart,
                                           uint16_t) {
  if (_huart == &huart1) {
    notifyWgsRxFromIsr();
  }
}

extern "C" void HAL_UART_TxCpltCallback(UART_HandleTypeDef *_huart) {
  if (_huart == &huart1) {
    notifyWgsTxCompleteFromIsr();
  }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *_huart) {
  if (_huart != &huart1) {
    return;
  }

  const uint32_t error = HAL_UART_GetError(_huart);
  if ((error & HAL_UART_ERROR_ORE) != 0U) {
    __HAL_UART_CLEAR_OREFLAG(_huart);
  }
  if ((error & HAL_UART_ERROR_FE) != 0U) {
    __HAL_UART_CLEAR_FEFLAG(_huart);
  }

  HAL_UARTEx_ReceiveToIdle_DMA(_huart, _huart->pRxBuffPtr, _huart->RxXferSize);
  __HAL_DMA_DISABLE_IT(_huart->hdmarx, DMA_IT_HT);
}
