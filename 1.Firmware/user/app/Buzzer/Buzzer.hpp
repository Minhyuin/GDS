#pragma once

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <cstdint>

namespace APP::BUZZER {

class BuzzerTask {
public:
  static constexpr uint32_t BEEP_MS = 20;

  BuzzerTask();

  bool init();
  void signal();

private:
  static constexpr uint32_t TASK_STACK_WORDS = 128;
  static constexpr UBaseType_t TASK_PRIORITY = tskIDLE_PRIORITY + 1;
  static constexpr UBaseType_t MAX_PENDING_BEEPS = 8;

  static void taskEntry(void *_argument);
  void taskLoop();

  StaticSemaphore_t semaphoreBuffer_{};
  SemaphoreHandle_t semaphore_{nullptr};
  StaticTask_t taskControlBlock_{};
  StackType_t taskStack_[TASK_STACK_WORDS]{};
  TaskHandle_t taskHandle_{nullptr};
};

void notifyProtocolCommandReceived();

} // namespace APP::BUZZER
