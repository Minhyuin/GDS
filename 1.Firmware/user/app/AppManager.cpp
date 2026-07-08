#include "AppManager.hpp"

#include "FreeRTOS.h"
#include "StmLog.hpp"
#include "task.h"

void AppManager::initApp() { createApp(); }

void AppManager::createApp() {
  // auto &buzzer = buzzer_.construct();
  // if (!buzzer.init()) {
  //   LOG::Logger::instance().warn(LOCATION, "App", "buzzer task init failed");
  // }

  auto &wgs = wgs_.construct();
  auto transportResult = wgs.init();
  if (!transportResult.ok()) {
    LOG::Logger::instance().warn(LOCATION, "App",
                                 "USART1 DMA transport start failed: %u",
                                 static_cast<unsigned>(transportResult.code));
  }

  const size_t freeHeap = xPortGetFreeHeapSize();
  const size_t minimumEverFreeHeap = xPortGetMinimumEverFreeHeapSize();
  LOG::Logger::instance().info(
      LOCATION, "App", "init complete, Free Heap: %lu, Min Ever: %lu",
      static_cast<unsigned long>(freeHeap),
      static_cast<unsigned long>(minimumEverFreeHeap));
}
