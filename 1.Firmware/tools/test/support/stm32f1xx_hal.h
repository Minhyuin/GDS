#pragma once
#include <cstddef>
#include <cstdint>
enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT };
struct TIM_TypeDef {
  uint32_t CR1{}, DIER{}, PSC{}, ARR{719}, CNT{}, EGR{}, SR{}, CCER{}, CCR1{},
      CCR2{}, CCR3{}, CCR4{}, CCMR1{0x6868}, CCMR2{0x6868};
};
struct TIM_HandleTypeDef {
  TIM_TypeDef *Instance;
  uint32_t State{1};
};
// Model BSRR atomic set/reset and remember whether the rise preceded CEN.
struct GPIO_TypeDef;
struct GpioBsrr {
  GPIO_TypeDef *port;
  uint32_t value{};
  void operator=(uint32_t bits);
};
struct GPIO_TypeDef {
  uint32_t ODR{}, CRL{}, CRH{};
  TIM_TypeDef *timer{};
  bool riseBeforeTimer{};
  void (*onRise)(){};
  GpioBsrr BSRR{this};
};
inline void GpioBsrr::operator=(uint32_t bits) {
  value = bits;
  if (bits & 0xffffU) {
    if (port->onRise) port->onRise();
    port->riseBeforeTimer = port->timer && !(port->timer->CR1 & 1U);
  }
  port->ODR = (port->ODR | (bits & 0xffffU)) & ~(bits >> 16U);
}
inline GPIO_TypeDef gpioA, gpioD;
inline GPIO_TypeDef *GPIOA = &gpioA, *GPIOD = &gpioD;
constexpr uint32_t GPIO_PIN_2 = 4, GPIO_PIN_3 = 8,
                   GPIO_PIN_12 = 0x1000, GPIO_PIN_13 = 0x2000;
constexpr uint32_t GPIO_MODE_OUTPUT_PP = 1, GPIO_MODE_AF_PP = 2,
                   GPIO_NOPULL = 0, GPIO_SPEED_FREQ_HIGH = 3;
struct GPIO_InitTypeDef { uint32_t Pin{}, Mode{}, Pull{}, Speed{}; };
inline void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init) {
  for (unsigned pin = 0; pin < 16; ++pin) {
    if (!(init->Pin & (1U << pin))) continue;
    auto &reg = pin < 8 ? port->CRL : port->CRH;
    unsigned shift = (pin % 8) * 4;
    uint32_t config = init->Speed | (init->Mode == GPIO_MODE_AF_PP ? 8U : 0U);
    reg = (reg & ~(15U << shift)) | (config << shift);
  }
}
struct RCC_TypeDef {
  uint32_t CFGR{};
};
inline RCC_TypeDef rcc;
inline RCC_TypeDef *RCC = &rcc;
inline uint32_t irqMask;
inline uint32_t __get_PRIMASK() { return irqMask; }
inline unsigned irqDisableCalls;
inline void __disable_irq() { ++irqDisableCalls; irqMask = 1; }
inline void __enable_irq() { irqMask = 0; }
constexpr uint32_t TIM_CHANNEL_1 = 0, TIM_CHANNEL_2 = 4, TIM_CHANNEL_3 = 8,
                   TIM_CHANNEL_4 = 12;
constexpr uint32_t TIM_CCER_CC1E = 1, TIM_CCER_CC2E = 16, TIM_CCER_CC3E = 256,
                   TIM_CCER_CC4E = 4096;
constexpr uint32_t TIM_CR1_CEN = 1, TIM_CR1_OPM = 8, TIM_DIER_UIE = 1,
                   TIM_EGR_UG = 1, TIM_FLAG_UPDATE = 1;
constexpr uint32_t TIM_CCMR1_OC1M = 0x70, TIM_CCMR1_OC2M = 0x7000;
constexpr uint32_t TIM_OCMODE_PWM2 = 0x70;
constexpr uint32_t HAL_TIM_STATE_READY = 1, HAL_TIM_STATE_BUSY = 2;
constexpr uint32_t RCC_CFGR_PPRE1 = 0x700, RCC_HCLK_DIV1 = 0;
constexpr uint32_t DAC_CHANNEL_1 = 0, DAC_CHANNEL_2 = 16;
inline uint32_t HAL_RCC_GetPCLK1Freq() { return 72000000; }
inline HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *t) {
  if (t->State != HAL_TIM_STATE_READY)
    return HAL_ERROR;
  t->Instance->CR1 |= TIM_CR1_CEN;
  t->State = HAL_TIM_STATE_BUSY;
  return HAL_OK;
}
// Match STM32F1 HAL: enabled PWM channels prevent Base_Stop from clearing CEN.
inline HAL_StatusTypeDef HAL_TIM_Base_Stop(TIM_HandleTypeDef *t) {
  if (!(t->Instance->CCER & 0x1111))
    t->Instance->CR1 &= ~TIM_CR1_CEN;
  t->State = HAL_TIM_STATE_READY;
  return HAL_OK;
}
#define __HAL_TIM_SET_COUNTER(t, v) ((t)->Instance->CNT = (v))
#define __HAL_TIM_GET_COUNTER(t) ((t)->Instance->CNT)
#define __HAL_TIM_SET_PRESCALER(t, v) ((t)->Instance->PSC = (v))
#define __HAL_TIM_SET_AUTORELOAD(t, v) ((t)->Instance->ARR = (v))
#define __HAL_TIM_CLEAR_FLAG(t, v) ((t)->Instance->SR &= ~(v))
#define __HAL_TIM_ENABLE(t) ((t)->Instance->CR1 |= TIM_CR1_CEN)
inline uint32_t &compare(TIM_HandleTypeDef *t, uint32_t c) {
  switch (c) {
  case 0:
    return t->Instance->CCR1;
  case 4:
    return t->Instance->CCR2;
  case 8:
    return t->Instance->CCR3;
  default:
    return t->Instance->CCR4;
  }
}
#define __HAL_TIM_SET_COMPARE(t, c, v) (compare(t, c) = (v))
struct DAC_TypeDef {
  uint32_t CR{};
  uint32_t holding[2]{}, output[2]{};
};
struct DAC_HandleTypeDef {
  DAC_TypeDef *Instance;
};
constexpr uint32_t DAC_CR_TEN1 = 4, DAC_ALIGN_12B_R = 0;
inline int dacIndex(uint32_t c) { return c == DAC_CHANNEL_1 ? 0 : 1; }
inline HAL_StatusTypeDef HAL_DAC_Stop_DMA(DAC_HandleTypeDef *, uint32_t) {
  return HAL_OK;
}
inline HAL_StatusTypeDef HAL_DAC_Start_DMA(DAC_HandleTypeDef *, uint32_t,
                                           const uint32_t *, uint32_t,
                                           uint32_t) {
  return HAL_OK;
}
inline HAL_StatusTypeDef HAL_DAC_Start(DAC_HandleTypeDef *, uint32_t) {
  // Enabling a channel does not latch a previously triggered DHR write.
  return HAL_OK;
}
inline HAL_StatusTypeDef HAL_DAC_SetValue(DAC_HandleTypeDef *d, uint32_t c,
                                          uint32_t, uint32_t v) {
  d->Instance->holding[dacIndex(c)] = v;
  if (!(d->Instance->CR & (DAC_CR_TEN1 << c)))
    d->Instance->output[dacIndex(c)] = v;
  return HAL_OK;
}
#define SET_BIT(reg, bit) ((reg) |= (bit))
#define CLEAR_BIT(reg, bit) ((reg) &= ~(bit))
