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

  LOG::Logger::instance().info(LOCATION, "GDS", "kernel objects ready");

  // vTaskStartScheduler(); // start scheduler in main.c -> osKernelStart()
}

void GDS::initBsp() { bspInit(); }

void GDS::initApp() { coreInit(); }

void GDS::init() {
  /* Welcome to GDS */
  initBsp();
  initApp();
}

void initGDS() { GDS::instance().init(); }

void initGDSBsp() { GDS::instance().initBsp(); }

void initGDSApp() { GDS::instance().initApp(); }
