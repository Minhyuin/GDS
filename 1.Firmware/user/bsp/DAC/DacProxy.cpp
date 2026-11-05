#include "DacProxy.hpp"

extern "C" {
#include "dac.h"
}

namespace BSP {

DAC_HandleTypeDef &DacProxy::dac() { return hdac; }

HAL_StatusTypeDef DacProxy::stopDma(uint32_t _channel) {
  return HAL_DAC_Stop_DMA(&dac(), _channel);
}

HAL_StatusTypeDef DacProxy::startDma(uint32_t _channel, const uint16_t *_buffer,
                                     uint32_t _length) {
  SET_BIT(dac().Instance->CR, DAC_CR_TEN1 << (_channel & 0x10U));
  return HAL_DAC_Start_DMA(&dac(), _channel,
                           reinterpret_cast<const uint32_t *>(_buffer), _length,
                           DAC_ALIGN_12B_R);
}

HAL_StatusTypeDef DacProxy::startDac(uint32_t _channel) {
  // Point/FIFO values latch directly; ACK timers must not delay the DAC update.
  CLEAR_BIT(dac().Instance->CR, DAC_CR_TEN1 << (_channel & 0x10U));
  return HAL_DAC_Start(&dac(), _channel);
}

HAL_StatusTypeDef DacProxy::setDacValue(uint32_t _channel, uint32_t _value) {
  return HAL_DAC_SetValue(&dac(), _channel, DAC_ALIGN_12B_R, _value);
}

} // namespace BSP
