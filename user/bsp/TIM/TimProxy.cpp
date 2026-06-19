#include "TimProxy.hpp"

extern "C" {
#include "tim.h"
}

namespace BSP {

TIM_HandleTypeDef &Tim6HandleProvider::handle() { return htim6; }

} // namespace BSP
