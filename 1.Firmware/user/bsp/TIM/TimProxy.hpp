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
    disableUpdateInterrupt();
    timer().Instance->CR1 &= ~TIM_CR1_OPM;
    setAckPulseTicks(ackPulseTicksForPeriod(currentPeriodTicks()));
    resetCounter();
    enableAckPwmOutputs();
    timer().Instance->EGR = TIM_EGR_UG;
    return start();
  }

  static HAL_StatusTypeDef stop() {
    disableUpdateInterrupt();
    stopAckPwm();
    return HAL_TIM_Base_Stop(&timer());
  }

  static HAL_StatusTypeDef stopContinuous() { return stop(); }

  static HAL_StatusTypeDef prepareSinglePulse(float _pulseWidthUs) {
    stop();
    const auto savedConfig = captureConfig();
    const auto status = configureSinglePulse(_pulseWidthUs);
    if (status != HAL_OK) {
      restoreConfig(savedConfig);
      return status;
    }

    singlePulseRestoreConfig_ = savedConfig;
    singlePulsePrepared_ = true;
    singlePulseFiring_ = false;

    resetCounter();
    enableOnePulseMode();
    disableUpdateInterrupt();
    stopAckPwm();

    timer().Instance->EGR = TIM_EGR_UG;
    clearUpdateFlag();
    preparePulseStart();
    return HAL_OK;
  }

  static HAL_StatusTypeDef firePreparedSinglePulseFromIsr() {
    if (!singlePulsePrepared_ || singlePulseFiring_) {
      return HAL_ERROR;
    }

    auto *registers = singlePulseRegisters_;
    singlePulseFiring_ = true;
    registers->CCER = singlePulseOutputEnable_;
    registers->CR1 |= TIM_CR1_CEN;
    return HAL_OK;
  }

  static bool completePreparedSinglePulseFromIsr(bool _keepPrepared = false) {
    if (!singlePulseFiring_) {
      disableUpdateInterrupt();
      clearUpdateFlag();
      return false;
    }

    disableUpdateInterrupt();
    clearUpdateFlag();
    stopAckPwm();
    if (!_keepPrepared) {
      restoreConfig(singlePulseRestoreConfig_);
      singlePulsePrepared_ = false;
    } else {
      resetCounter();
      preparePulseStart();
    }
    singlePulseFiring_ = false;
    if (!_keepPrepared) {
      timer().State = HAL_TIM_STATE_READY;
    }
    return true;
  }

  static void cancelPreparedSinglePulse() {
    if (!singlePulsePrepared_) {
      return;
    }
    stop();
    clearUpdateFlag();
    restoreConfig(singlePulseRestoreConfig_);
    singlePulsePrepared_ = false;
    singlePulseFiring_ = false;
  }

  static void resetCounter() { __HAL_TIM_SET_COUNTER(&timer(), 0U); }

  static void startCounterNow() {
    disableUpdateInterrupt();
    timer().State = HAL_TIM_STATE_BUSY;
    setAckPulseTicks(ackPulseTicksForPeriod(currentPeriodTicks()));
    enableAckPwmOutputs();
    timer().Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_ENABLE(&timer());
  }

  static HAL_StatusTypeDef setUpdateFrequencyHz(float _frequencyHz) {
    disableUpdateInterrupt();
    if (!std::isfinite(_frequencyHz) || _frequencyHz < 1.0F ||
        _frequencyHz > static_cast<float>(timerClockHz())) {
      return HAL_ERROR;
    }

    const uint32_t timerClock = timerClockHz();
    const auto target = static_cast<uint32_t>(_frequencyHz + 0.5F);
    if (target == 0U || target > timerClock) {
      return HAL_ERROR;
    }

    const uint64_t divisor = static_cast<uint64_t>(target) * 65536U;
    uint32_t prescaler = static_cast<uint32_t>(
        (static_cast<uint64_t>(timerClock) + divisor - 1U) / divisor);
    const uint32_t period = (timerClock / prescaler) / target;
    if (period < 2U || period > 65536UL) {
      return HAL_ERROR;
    }

    const bool wasEnabled = (timer().Instance->CR1 & TIM_CR1_CEN) != 0U;
    if (wasEnabled) {
      // Keep PWM channel enables, but stop the counter while changing its base.
      timer().Instance->CR1 &= ~TIM_CR1_CEN;
    }

    __HAL_TIM_SET_PRESCALER(&timer(), prescaler - 1U);
    __HAL_TIM_SET_AUTORELOAD(&timer(), period - 1U);
    setAckPulseTicks(ackPulseTicksForPeriod(period));
    __HAL_TIM_SET_COUNTER(&timer(), 0U);
    timer().Instance->EGR = TIM_EGR_UG;

    if (wasEnabled) {
      // The HAL state stayed BUSY while only CEN was paused.
      __HAL_TIM_ENABLE(&timer());
    }
    return HAL_OK;
  }

private:
  static void enableAckPwmOutputs() {
    timer().Instance->CCER |= channelEnableBit(HandleProvider::ACK_CHANNEL_1);
    timer().Instance->CCER |= channelEnableBit(HandleProvider::ACK_CHANNEL_2);
  }

  static void enableAckPwmOutputsTogether() {
    timer().Instance->CCER |= channelEnableBit(HandleProvider::ACK_CHANNEL_1) |
                              channelEnableBit(HandleProvider::ACK_CHANNEL_2);
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
    uint32_t control1{0U};
    uint32_t interruptEnable{0U};
    uint32_t prescaler{0U};
    uint32_t autoReload{0U};
    uint32_t compare1{0U};
    uint32_t compare2{0U};
    uint32_t counter{0U};
    uint32_t captureCompareMode1{0U};
    uint32_t captureCompareMode2{0U};
  };

  static inline TimerConfig_s singlePulseRestoreConfig_{};
  static inline bool singlePulsePrepared_{false};
  static inline bool singlePulseFiring_{false};
  static inline TIM_TypeDef *singlePulseRegisters_{};
  static inline uint32_t singlePulseOutputEnable_{};

  static void preparePulseStart() {
    auto &handle = timer();
    singlePulseRegisters_ = handle.Instance;
    // Keep outputs disabled until the external trigger; cache the enable value.
    singlePulseOutputEnable_ =
        singlePulseRegisters_->CCER |
        channelEnableBit(HandleProvider::ACK_CHANNEL_1) |
        channelEnableBit(HandleProvider::ACK_CHANNEL_2);
    handle.State = HAL_TIM_STATE_BUSY;
    singlePulseRegisters_->SR = ~TIM_FLAG_UPDATE;
    singlePulseRegisters_->DIER |= TIM_DIER_UIE;
  }

  static TimerConfig_s captureConfig() {
    return TimerConfig_s{
        .control1 = timer().Instance->CR1,
        .interruptEnable = timer().Instance->DIER,
        .prescaler = timer().Instance->PSC,
        .autoReload = timer().Instance->ARR,
        .compare1 = compareValue(HandleProvider::ACK_CHANNEL_1),
        .compare2 = compareValue(HandleProvider::ACK_CHANNEL_2),
        .counter = __HAL_TIM_GET_COUNTER(&timer()),
        .captureCompareMode1 = timer().Instance->CCMR1,
        .captureCompareMode2 = timer().Instance->CCMR2,
    };
  }

  static void restoreConfig(const TimerConfig_s &_config) {
    timer().Instance->CCMR1 = _config.captureCompareMode1;
    timer().Instance->CCMR2 = _config.captureCompareMode2;
    __HAL_TIM_SET_PRESCALER(&timer(), _config.prescaler);
    __HAL_TIM_SET_AUTORELOAD(&timer(), _config.autoReload);
    setCompareValue(HandleProvider::ACK_CHANNEL_1, _config.compare1);
    setCompareValue(HandleProvider::ACK_CHANNEL_2, _config.compare2);
    __HAL_TIM_SET_COUNTER(&timer(), _config.counter);
    timer().Instance->EGR = TIM_EGR_UG;
    __HAL_TIM_CLEAR_FLAG(&timer(), TIM_FLAG_UPDATE);
    timer().Instance->DIER = _config.interruptEnable;
    timer().Instance->CR1 = _config.control1;
  }

  static HAL_StatusTypeDef configureSinglePulse(float _pulseWidthUs) {
    if (!std::isfinite(_pulseWidthUs) || _pulseWidthUs <= 0.0F) {
      return HAL_ERROR;
    }

    const uint32_t timerClock = timerClockHz();
    const double pulseSeconds = static_cast<double>(_pulseWidthUs) / 1000000.0;
    uint32_t prescaler = static_cast<uint32_t>(
        std::ceil((static_cast<double>(timerClock) * pulseSeconds) / 65535.0));
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

    // PWM2 stays low at CNT=0, rises at 1, and falls at overflow.
    // OPM then stops at CNT=0: the falling edge never waits for the ISR.
    auto *registers = timer().Instance;
    auto &mode = HandleProvider::ACK_CHANNEL_1 == TIM_CHANNEL_1
                     ? registers->CCMR1 : registers->CCMR2;
    mode = (mode & ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC2M)) |
           TIM_OCMODE_PWM2 | (TIM_OCMODE_PWM2 << 8U);
    __HAL_TIM_SET_PRESCALER(&timer(), prescaler - 1U);
    __HAL_TIM_SET_AUTORELOAD(&timer(), pulseTicks);
    setAckPulseTicks(1U);
    return HAL_OK;
  }

  static void enableOnePulseMode() { timer().Instance->CR1 |= TIM_CR1_OPM; }

  static void enableUpdateInterrupt() {
    timer().Instance->DIER |= TIM_DIER_UIE;
  }

  static void disableUpdateInterrupt() {
    timer().Instance->DIER &= ~TIM_DIER_UIE;
  }

  static void clearUpdateFlag() {
    __HAL_TIM_CLEAR_FLAG(&timer(), TIM_FLAG_UPDATE);
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
