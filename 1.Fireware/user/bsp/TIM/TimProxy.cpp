#include "TimProxy.hpp"

extern "C" {
#include "tim.h"
}

namespace BSP {

TIM_HandleTypeDef &Tim6HandleProvider::handle() { return htim6; }

TIM_HandleTypeDef &Tim7HandleProvider::handle() { return htim7; }

} // namespace BSP
