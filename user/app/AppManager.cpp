#include "AppManager.hpp"

#include "FreeRTOS.h"
#include "StmLog.hpp"
#include "task.h"

void AppManager::initApp() { createApp(); }

void AppManager::createApp() {
  auto &wgs = wgs_.construct();
  auto transportResult = wgs.init();
  if (!transportResult.ok()) {
    LOG::Logger::instance().warn(LOCATION, "App",
                                 "USART1 DMA transport start failed: %u",
                                 static_cast<unsigned>(transportResult.code));
  }

  uint32_t freeHeap = xPortGetFreeHeapSize();
  LOG::Logger::instance().info(LOCATION, "App", "init complete, Free Heap: %u",
                               freeHeap);
}
