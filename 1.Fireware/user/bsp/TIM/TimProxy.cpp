#include "TimProxy.hpp"

extern "C" {
#include "tim.h"
}

namespace BSP {

TIM_HandleTypeDef &Tim4HandleProvider::handle() { return htim4; }

TIM_HandleTypeDef &Tim5HandleProvider::handle() { return htim5; }

} // namespace BSP
