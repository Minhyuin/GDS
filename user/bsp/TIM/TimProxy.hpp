#pragma once

#include "stm32f1xx_hal.h"

namespace BSP {

class Tim6HandleProvider {
public:
  static TIM_HandleTypeDef &handle();
};

template <typename HandleProvider> class TimProxy {
public:
  static TIM_HandleTypeDef &timer() { return HandleProvider::handle(); }

  static HAL_StatusTypeDef start() { return HAL_TIM_Base_Start(&timer()); }

  static HAL_StatusTypeDef stop() { return HAL_TIM_Base_Stop(&timer()); }
};

} // namespace BSP
