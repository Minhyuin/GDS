#pragma once

[[noreturn]] void testDisableInterruptsForOom();

#define taskDISABLE_INTERRUPTS() testDisableInterruptsForOom()
