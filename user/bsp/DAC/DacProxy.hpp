#pragma once

#include "stm32f1xx_hal.h"

#include <cstddef>
#include <cstdint>

namespace BSP {

class DacProxy {
public:
  static DAC_HandleTypeDef &dac();

  static HAL_StatusTypeDef stopDma(uint32_t _channel);
  static HAL_StatusTypeDef startDma(uint32_t _channel, const uint16_t *_buffer,
                                    uint32_t _length);
  static HAL_StatusTypeDef startDac(uint32_t _channel);
  static HAL_StatusTypeDef setDacValue(uint32_t _channel, uint32_t _value);
};

} // namespace BSP
