#include "WGS.hpp"

#include "usart.h"

#include <cmath>
#include <cstring>

namespace APP::WGS {

namespace {
WGSManager *activeWgs = nullptr;
constexpr float MAX_OUTPUT_VOLTAGE = 3.3F;
constexpr size_t POINT_SET_PAYLOAD_SIZE = 9;
constexpr size_t SEQUENCE_SET_HEADER_SIZE = 2;
constexpr size_t SEQUENCE_POINT_SIZE = 8;

uint32_t readU32(std::span<const std::byte> _bytes, size_t _offset) {
  return static_cast<uint32_t>(
      static_cast<uint8_t>(_bytes[_offset]) |
      (static_cast<uint16_t>(static_cast<uint8_t>(_bytes[_offset + 1])) << 8U) |
      (static_cast<uint32_t>(static_cast<uint8_t>(_bytes[_offset + 2]))
       << 16U) |
      (static_cast<uint32_t>(static_cast<uint8_t>(_bytes[_offset + 3]))
       << 24U));
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

void WGSManager::onDacDmaHalfCompleteFromIsr() {
  output_.onDmaHalfCompleteFromIsr();
}

void WGSManager::onDacDmaCompleteFromIsr() {
  output_.onDmaCompleteFromIsr();
}

PROTOCOL::Result_s WGSManager::startTask() {
  taskHandle_ =
      xTaskCreateStatic(taskEntry, "WGS_RX", TASK_STACK_WORDS, this,
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
  case PROTOCOL::CommandType_e::POINT_SET: {
    if (_frame.payload.size() != POINT_SET_PAYLOAD_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }
    const float xVoltage = readFloat32Le(_frame.payload, 1);
    const float yVoltage = readFloat32Le(_frame.payload, 5);

    _command = PROTOCOL::PointSetCommand_s{
        .xVoltage = xVoltage,
        .yVoltage = yVoltage,
    };
    return PROTOCOL::Result_s::success();
  }
  case PROTOCOL::CommandType_e::SEQUENCE_SET: {
    if (_frame.payload.size() < SEQUENCE_SET_HEADER_SIZE) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const size_t pointCount = static_cast<uint8_t>(_frame.payload[1]);
    if (pointCount == 0U || pointCount > decodeScratch_.size()) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    const size_t expectedPayloadSize =
        SEQUENCE_SET_HEADER_SIZE + pointCount * SEQUENCE_POINT_SIZE;
    if (_frame.payload.size() != expectedPayloadSize) {
      return PROTOCOL::Result_s::fail(PROTOCOL::ErrorCode_e::BAD_LENGTH);
    }

    size_t offset = SEQUENCE_SET_HEADER_SIZE;
    for (size_t i = 0; i < pointCount; ++i) {
      const float xVoltage = readFloat32Le(_frame.payload, offset);
      const float yVoltage =
          readFloat32Le(_frame.payload, offset + sizeof(float));
      decodeScratch_[i] = PROTOCOL::DacCodePoint_s{
          .x = voltageToDacCode(xVoltage),
          .y = voltageToDacCode(yVoltage),
      };
      offset += SEQUENCE_POINT_SIZE;
    }

    _command = PROTOCOL::SequenceSetCommand_s{
        .points = std::span<const PROTOCOL::DacCodePoint_s>(
            decodeScratch_.data(), pointCount),
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
                                           uint16_t _size) {
  if (_huart == &huart1) {
    notifyWgsRxFromIsr();
  }
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef *_huart) {
  if (HAL_UART_GetError(_huart) == HAL_UART_ERROR_ORE) {
    __HAL_UART_CLEAR_OREFLAG(_huart);
    HAL_UARTEx_ReceiveToIdle_DMA(_huart, _huart->pRxBuffPtr,
                                 _huart->RxXferSize);
    __HAL_DMA_DISABLE_IT(_huart->hdmarx, DMA_IT_HT);
  }
  // https://blog.csdn.net/Jonshen/article/details/144032215
  // (上电初始化出现帧错误（FE）)
  else if (HAL_UART_GetError(_huart) == HAL_UART_ERROR_FE) {
    __HAL_UART_CLEAR_FEFLAG(_huart);
    HAL_UARTEx_ReceiveToIdle_DMA(_huart, _huart->pRxBuffPtr,
                                 _huart->RxXferSize);
    __HAL_DMA_DISABLE_IT(_huart->hdmarx, DMA_IT_HT);
  }
}
