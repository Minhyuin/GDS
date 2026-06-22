#pragma once

#include "stm32f1xx_hal.h"

#include <cmath>

namespace BSP {

class Tim4HandleProvider {
public:
  static TIM_HandleTypeDef &handle();
  static constexpr uint32_t ACK_CHANNEL_1 = TIM_CHANNEL_1;
  static constexpr uint32_t ACK_CHANNEL_2 = TIM_CHANNEL_2;
};

class Tim5HandleProvider {
public:
  static TIM_HandleTypeDef &handle();
  static constexpr uint32_t ACK_CHANNEL_1 = TIM_CHANNEL_3;
  static constexpr uint32_t ACK_CHANNEL_2 = TIM_CHANNEL_4;
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

  static HAL_StatusTypeDef startContinuous() {
    setAckPulseTicks(ackPulseTicksForPeriod(currentPeriodTicks()));
    resetCounter();
    enableAckPwmOutputs();
    timer().Instance->EGR = TIM_EGR_UG;
    return start();
  }

  static HAL_StatusTypeDef stop() {
    if ((timer().Instance->CR1 & TIM_CR1_CEN) == 0U) {
      return stopAckPwm();
    }

    const auto timerStatus = HAL_TIM_Base_Stop(&timer());
    const auto pwmStatus = stopAckPwm();
    return timerStatus != HAL_OK ? timerStatus : pwmStatus;
  }

  static HAL_StatusTypeDef stopContinuous() { return stop(); }

  static HAL_StatusTypeDef pulseOnce(float _pulseWidthUs) {
    auto status = stop();
    if (status != HAL_OK) {
      return status;
    }

    const auto savedConfig = captureConfig();
    uint32_t pulseTicks = 0U;
    status = configureSinglePulse(_pulseWidthUs, pulseTicks);
    if (status != HAL_OK) {
      restoreConfig(savedConfig);
      return status;
    }

    resetCounter();
    enableAckPwmOutputs();

    timer().Instance->EGR = TIM_EGR_UG;
    status = HAL_TIM_Base_Start(&timer());
    if (status != HAL_OK) {
      stopAckPwm();
      restoreConfig(savedConfig);
      return status;
    }

    while (__HAL_TIM_GET_COUNTER(&timer()) < pulseTicks) {
    }

    const auto timerStatus = HAL_TIM_Base_Stop(&timer());
    const auto pwmStatus = stopAckPwm();
    restoreConfig(savedConfig);
    return timerStatus != HAL_OK ? timerStatus : pwmStatus;
  }

  static void resetCounter() { __HAL_TIM_SET_COUNTER(&timer(), 0U); }

  static void startCounterNow() {
    timer().State = HAL_TIM_STATE_BUSY;
    setAckPulseTicks(ackPulseTicksForPeriod(currentPeriodTicks()));
    enableAckPwmOutputs();
    timer().Instance->EGR = TIM_EGR_UG;
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
    if (period < 2U) {
      return HAL_ERROR;
    }

    const bool wasEnabled = (timer().Instance->CR1 & TIM_CR1_CEN) != 0U;
    if (wasEnabled) {
      HAL_TIM_Base_Stop(&timer());
    }

    __HAL_TIM_SET_PRESCALER(&timer(), prescaler - 1U);
    __HAL_TIM_SET_AUTORELOAD(&timer(), period - 1U);
    setAckPulseTicks(ackPulseTicksForPeriod(period));
    __HAL_TIM_SET_COUNTER(&timer(), 0U);
    timer().Instance->EGR = TIM_EGR_UG;

    if (wasEnabled) {
      return HAL_TIM_Base_Start(&timer());
    }
    return HAL_OK;
  }

private:
  static void enableAckPwmOutputs() {
    timer().Instance->CCER |= channelEnableBit(HandleProvider::ACK_CHANNEL_1);
    timer().Instance->CCER |= channelEnableBit(HandleProvider::ACK_CHANNEL_2);
  }

  static HAL_StatusTypeDef stopAckPwm() {
    timer().Instance->CCER &= ~channelEnableBit(HandleProvider::ACK_CHANNEL_1);
    timer().Instance->CCER &= ~channelEnableBit(HandleProvider::ACK_CHANNEL_2);
    return HAL_OK;
  }

  static uint32_t channelEnableBit(uint32_t _channel) {
    switch (_channel) {
    case TIM_CHANNEL_1:
      return TIM_CCER_CC1E;
    case TIM_CHANNEL_2:
      return TIM_CCER_CC2E;
    case TIM_CHANNEL_3:
      return TIM_CCER_CC3E;
    case TIM_CHANNEL_4:
      return TIM_CCER_CC4E;
    default:
      return 0U;
    }
  }

  struct TimerConfig_s {
    uint32_t prescaler{0U};
    uint32_t autoReload{0U};
    uint32_t compare1{0U};
    uint32_t compare2{0U};
    uint32_t counter{0U};
  };

  static TimerConfig_s captureConfig() {
    return TimerConfig_s{
        .prescaler = timer().Instance->PSC,
        .autoReload = timer().Instance->ARR,
        .compare1 = compareValue(HandleProvider::ACK_CHANNEL_1),
        .compare2 = compareValue(HandleProvider::ACK_CHANNEL_2),
        .counter = __HAL_TIM_GET_COUNTER(&timer()),
    };
  }

  static void restoreConfig(const TimerConfig_s &_config) {
    __HAL_TIM_SET_PRESCALER(&timer(), _config.prescaler);
    __HAL_TIM_SET_AUTORELOAD(&timer(), _config.autoReload);
    setCompareValue(HandleProvider::ACK_CHANNEL_1, _config.compare1);
    setCompareValue(HandleProvider::ACK_CHANNEL_2, _config.compare2);
    __HAL_TIM_SET_COUNTER(&timer(), _config.counter);
    timer().Instance->EGR = TIM_EGR_UG;
  }

  static HAL_StatusTypeDef configureSinglePulse(float _pulseWidthUs,
                                                uint32_t &_pulseTicks) {
    if (!std::isfinite(_pulseWidthUs) || _pulseWidthUs <= 0.0F) {
      return HAL_ERROR;
    }

    const uint32_t timerClock = timerClockHz();
    const double pulseSeconds =
        static_cast<double>(_pulseWidthUs) / 1000000.0;
    uint32_t prescaler = static_cast<uint32_t>(
        std::ceil((static_cast<double>(timerClock) * pulseSeconds) /
                  65535.0));
    if (prescaler == 0U) {
      prescaler = 1U;
    }
    if (prescaler > 65536UL) {
      return HAL_ERROR;
    }

    uint32_t pulseTicks = 0U;
    while (prescaler <= 65536UL) {
      const uint32_t tickHz = timerClock / prescaler;
      pulseTicks = static_cast<uint32_t>(
          std::ceil(static_cast<double>(tickHz) * pulseSeconds));
      if (pulseTicks == 0U) {
        pulseTicks = 1U;
      }
      if (pulseTicks <= 65535U) {
        break;
      }
      ++prescaler;
    }

    if (prescaler > 65536UL || pulseTicks == 0U || pulseTicks > 65535U) {
      return HAL_ERROR;
    }

    const uint32_t periodTicks =
        pulseTicks == 65535U ? 65536U : pulseTicks + 1U;
    __HAL_TIM_SET_PRESCALER(&timer(), prescaler - 1U);
    __HAL_TIM_SET_AUTORELOAD(&timer(), periodTicks - 1U);
    setAckPulseTicks(pulseTicks);
    _pulseTicks = pulseTicks;
    return HAL_OK;
  }

  static uint32_t ackPulseTicksForPeriod(uint32_t _periodTicks) {
    if (_periodTicks <= 2U) {
      return 1U;
    }
    return _periodTicks / 2U;
  }

  static uint32_t currentPeriodTicks() {
    return static_cast<uint32_t>(timer().Instance->ARR) + 1U;
  }

  static void setAckPulseTicks(uint32_t _ticks) {
    __HAL_TIM_SET_COMPARE(&timer(), HandleProvider::ACK_CHANNEL_1, _ticks);
    __HAL_TIM_SET_COMPARE(&timer(), HandleProvider::ACK_CHANNEL_2, _ticks);
  }

  static uint32_t compareValue(uint32_t _channel) {
    switch (_channel) {
    case TIM_CHANNEL_1:
      return timer().Instance->CCR1;
    case TIM_CHANNEL_2:
      return timer().Instance->CCR2;
    case TIM_CHANNEL_3:
      return timer().Instance->CCR3;
    case TIM_CHANNEL_4:
      return timer().Instance->CCR4;
    default:
      return 0U;
    }
  }

  static void setCompareValue(uint32_t _channel, uint32_t _ticks) {
    switch (_channel) {
    case TIM_CHANNEL_1:
      timer().Instance->CCR1 = _ticks;
      break;
    case TIM_CHANNEL_2:
      timer().Instance->CCR2 = _ticks;
      break;
    case TIM_CHANNEL_3:
      timer().Instance->CCR3 = _ticks;
      break;
    case TIM_CHANNEL_4:
      timer().Instance->CCR4 = _ticks;
      break;
    default:
      break;
    }
  }

  static uint32_t timerClockHz() {
    const uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();
    const uint32_t ppre1 = (RCC->CFGR & RCC_CFGR_PPRE1);
    return ppre1 == RCC_HCLK_DIV1 ? pclk1 : pclk1 * 2U;
  }
};

} // namespace BSP
