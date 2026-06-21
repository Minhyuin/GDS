#pragma once

#include "stm32f1xx_hal.h"

namespace BSP {

class Tim6HandleProvider {
public:
  static TIM_HandleTypeDef &handle();
};

class Tim7HandleProvider {
public:
  static TIM_HandleTypeDef &handle();
};

template <typename HandleProvider> class TimProxy {
public:
  static TIM_HandleTypeDef &timer() { return HandleProvider::handle(); }

  static HAL_StatusTypeDef start() {
    if ((timer().Instance->CR1 & TIM_CR1_CEN) != 0U) {
      return HAL_OK;
    }
    return HAL_TIM_Base_Start(&timer());
  }

  static HAL_StatusTypeDef stop() {
    if ((timer().Instance->CR1 & TIM_CR1_CEN) == 0U) {
      return HAL_OK;
    }
    return HAL_TIM_Base_Stop(&timer());
  }

  static void resetCounter() { __HAL_TIM_SET_COUNTER(&timer(), 0U); }

  static void startCounterNow() {
    timer().State = HAL_TIM_STATE_BUSY;
    __HAL_TIM_ENABLE(&timer());
  }

  static HAL_StatusTypeDef setUpdateFrequencyHz(float _frequencyHz) {
    if (_frequencyHz <= 0.0F) {
      return HAL_ERROR;
    }

    const uint32_t timerClock = timerClockHz();
    const auto target = static_cast<uint32_t>(_frequencyHz + 0.5F);
    if (target == 0U || target > timerClock) {
      return HAL_ERROR;
    }

    uint32_t prescaler = (timerClock + (target * 65536UL) - 1UL) /
                         (target * 65536UL);
    if (prescaler == 0U) {
      prescaler = 1U;
    }
    if (prescaler > 65536UL) {
      return HAL_ERROR;
    }

    const uint32_t period = (timerClock / prescaler) / target;
    if (period == 0U || period > 65536UL) {
      return HAL_ERROR;
    }

    const bool wasEnabled = (timer().Instance->CR1 & TIM_CR1_CEN) != 0U;
    if (wasEnabled) {
      HAL_TIM_Base_Stop(&timer());
    }

    __HAL_TIM_SET_PRESCALER(&timer(), prescaler - 1U);
    __HAL_TIM_SET_AUTORELOAD(&timer(), period - 1U);
    __HAL_TIM_SET_COUNTER(&timer(), 0U);
    timer().Instance->EGR = TIM_EGR_UG;

    if (wasEnabled) {
      return HAL_TIM_Base_Start(&timer());
    }
    return HAL_OK;
  }

private:
  static uint32_t timerClockHz() {
    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1);
    return ppre1 == RCC_HCLK_DIV1 ? pclk1 : pclk1 * 2U;
  }
};

} // namespace BSP
