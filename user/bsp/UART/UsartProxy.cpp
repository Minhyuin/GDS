#include "UsartProxy.hpp"

extern "C" {
#include "usart.h"

extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
}

namespace BSP {

UART_HandleTypeDef &Usart1HandleProvider::uart() { return huart1; }

DMA_HandleTypeDef &Usart1HandleProvider::rxDma() { return hdma_usart1_rx; }

DMA_HandleTypeDef &Usart1HandleProvider::txDma() { return hdma_usart1_tx; }

} // namespace BSP
