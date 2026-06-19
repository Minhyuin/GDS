file(GLOB_RECURSE SOURCES CMAKE_CONFIGURE_DEPENDS 
  "${THIRD_PARTY_DIR}/RTT/RTT/*.c"
  "${THIRD_PARTY_DIR}/RTT/RTT/*.S"
  "${THIRD_PARTY_DIR}/RTT/Syscalls/SEGGER_RTT_Syscalls_GCC.c"
  "${THIRD_PARTY_DIR}/systemview/SYSVIEW/SEGGER_SYSVIEW.c"
  "${THIRD_PARTY_DIR}/systemview/Sample/FreeRTOSV10/SEGGER_SYSVIEW_FreeRTOS.c"
  "${THIRD_PARTY_DIR}/systemview/Sample/FreeRTOSV10/Config/Cortex-M/SEGGER_SYSVIEW_Config_FreeRTOS.c")

add_library(segger STATIC ${SOURCES})

target_link_libraries(segger PUBLIC stm32cubemx)

target_include_directories(segger PUBLIC
  "${THIRD_PARTY_DIR}/RTT/Config"
  "${THIRD_PARTY_DIR}/RTT/RTT"
  "${THIRD_PARTY_DIR}/systemview/Config"
  "${THIRD_PARTY_DIR}/systemview/SEGGER"
  "${THIRD_PARTY_DIR}/systemview/SYSVIEW"
  "${THIRD_PARTY_DIR}/systemview/Sample/FreeRTOSV10"
  "${THIRD_PARTY_DIR}/systemview/Sample/FreeRTOSV10/Config/Cortex-M")

target_include_directories(FreeRTOS PUBLIC
  "${THIRD_PARTY_DIR}/RTT/Config"
  "${THIRD_PARTY_DIR}/RTT/RTT"
  "${THIRD_PARTY_DIR}/systemview/Config"
  "${THIRD_PARTY_DIR}/systemview/SEGGER"
  "${THIRD_PARTY_DIR}/systemview/SYSVIEW"
  "${THIRD_PARTY_DIR}/systemview/Sample/FreeRTOSV10")
