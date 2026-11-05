#pragma once

#include <cstddef>

extern "C" {
void *pvPortMalloc(std::size_t _size);
void vPortFree(void *_ptr);
}

#define portBYTE_ALIGNMENT 8
