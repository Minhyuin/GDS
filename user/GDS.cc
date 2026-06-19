#include "GDS.hpp"
#include "FreeRTOS.h"
#include "SEGGER_SYSVIEW.h"
#include "StmLog.hpp"
#include "task.h"

#include "AppManager.hpp"

void GDS::bspInit() {
  // sysview init must be before segger RTT init (in log)
  LOG::Logger::instance();
  SEGGER_SYSVIEW_Conf();
}

void GDS::coreInit() {
  AppManager::instance().initApp();

  LOG::Logger::instance().info(LOCATION, "GDS", "kernal start");

  // vTaskStartScheduler(); // start scheduler in main.c -> osKernelStart()
}

void GDS::init() {
  /* Welcome to GDS */
  bspInit();
  coreInit();
}

void initGDS() { GDS::instance().init(); }
