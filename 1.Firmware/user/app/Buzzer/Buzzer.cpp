#include "Buzzer.hpp"

#include "main.h"

namespace APP::BUZZER {

namespace {
BuzzerTask *activeBuzzer = nullptr;
constexpr uint32_t BEEP_GAP_MS = 10;
} // namespace

BuzzerTask::BuzzerTask() { activeBuzzer = this; }

bool BuzzerTask::init() {
  HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);

  semaphore_ =
      xSemaphoreCreateCountingStatic(MAX_PENDING_BEEPS, 0, &semaphoreBuffer_);
  if (semaphore_ == nullptr) {
    return false;
  }

  taskHandle_ =
      xTaskCreateStatic(taskEntry, "BUZZER", TASK_STACK_WORDS, this,
                        TASK_PRIORITY, taskStack_, &taskControlBlock_);
  return taskHandle_ != nullptr;
}

void BuzzerTask::signal() {
  // if (semaphore_ == nullptr) {
  //   return;
  // }
  // (void)xSemaphoreGive(semaphore_);
}

void BuzzerTask::taskEntry(void *_argument) {
  auto *self = static_cast<BuzzerTask *>(_argument);
  self->taskLoop();
}

void BuzzerTask::taskLoop() {
  while (true) {
    if (xSemaphoreTake(semaphore_, portMAX_DELAY) != pdTRUE) {
      continue;
    }

    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(BEEP_MS));
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(BEEP_GAP_MS));
  }
}

void notifyProtocolCommandReceived() {
  if (activeBuzzer != nullptr) {
    activeBuzzer->signal();
  }
}

} // namespace APP::BUZZER
