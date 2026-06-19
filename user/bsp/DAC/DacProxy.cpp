#include "DacProxy.hpp"

extern "C" {
#include "dac.h"
}

namespace BSP {

DAC_HandleTypeDef &DacProxy::dac() { return hdac; }

HAL_StatusTypeDef DacProxy::stopDma(uint32_t _channel) {
  return HAL_DAC_Stop_DMA(&dac(), _channel);
}

HAL_StatusTypeDef DacProxy::startDma(uint32_t _channel,
                                     const uint32_t *_buffer,
                                     uint32_t _length) {
  return HAL_DAC_Start_DMA(&dac(), _channel, _buffer, _length,
                           DAC_ALIGN_12B_R);
}

HAL_StatusTypeDef DacProxy::startDac(uint32_t _channel) {
  return HAL_DAC_Start(&dac(), _channel);
}

HAL_StatusTypeDef DacProxy::setDacValue(uint32_t _channel, uint32_t _value) {
  return HAL_DAC_SetValue(&dac(), _channel, DAC_ALIGN_12B_R, _value);
}

} // namespace BSP
