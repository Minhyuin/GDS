#pragma once

#include "stm32f1xx_hal.h"

#include <cstddef>
#include <cstdint>

namespace BSP {

class Usart1HandleProvider {
public:
  static UART_HandleTypeDef &uart();
  static DMA_HandleTypeDef &rxDma();
  static DMA_HandleTypeDef &txDma();
};

template <typename HandleProvider> class UsartProxy {
public:
  static UART_HandleTypeDef &uart() { return HandleProvider::uart(); }

  static DMA_HandleTypeDef &rxDma() { return HandleProvider::rxDma(); }

  static DMA_HandleTypeDef &txDma() { return HandleProvider::txDma(); }

  static HAL_StatusTypeDef startRxDma(uint8_t *_buffer, size_t _size) {
    return HAL_UART_Receive_DMA(&uart(), _buffer, _size);
  }

  static HAL_StatusTypeDef transmitDma(uint8_t *_buffer, size_t _size) {
    return HAL_UART_Transmit_DMA(&uart(), _buffer, _size);
  }

  static void stopDma() { HAL_UART_DMAStop(&uart()); }

  static size_t rxDmaRemaining() { return __HAL_DMA_GET_COUNTER(&rxDma()); }
};

} // namespace BSP
