#include "FreeRTOS.h"
#include "GDS.hpp"
#include "SEGGER_SYSVIEW.h"
#include "StmLog.hpp"
#include "task.h"

void GDS::bspInit() {
  // sysview init must be before segger RTT init (in log)
  LOG::Logger::instance();
  SEGGER_SYSVIEW_Conf();
}

void GDS::coreInit() {
  LOG::info("GDS", "kernal start");

  // vTaskStartScheduler(); // start scheduler in main.c -> osKernelStart()
}

void GDS::init() {
  /* Welcome to GDS */
  bspInit();
  coreInit();
}

void initGDS() { GDS::instance().init(); }
