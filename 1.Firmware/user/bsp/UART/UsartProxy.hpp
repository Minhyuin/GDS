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
    return HAL_UARTEx_ReceiveToIdle_DMA(&uart(), _buffer, _size);
  }

  static HAL_StatusTypeDef transmitBlocking(uint8_t *_buffer, size_t _size,
                                            uint32_t _timeoutMs) {
    return HAL_UART_Transmit(&uart(), _buffer, static_cast<uint16_t>(_size),
                             _timeoutMs);
  }

  static uint32_t irqSave() {
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
  }

  static void irqRestore(uint32_t _primask) { __set_PRIMASK(_primask); }

  static void stopDma() { HAL_UART_DMAStop(&uart()); }

  static size_t rxDmaRemaining() { return __HAL_DMA_GET_COUNTER(&rxDma()); }
};

} // namespace BSP
