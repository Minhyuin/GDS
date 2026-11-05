#include <cstddef>
#include <cstdint>
#include <new>

#include "FreeRTOS.h"
#include "task.h"

namespace {
constexpr std::size_t kPortAlignment = portBYTE_ALIGNMENT;

[[noreturn]] void allocationFailed() noexcept
{
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void *allocate(std::size_t _size) noexcept
{
    return pvPortMalloc(_size == 0U ? 1U : _size);
}

void *allocateAligned(std::size_t _size, std::size_t _alignment) noexcept
{
    if (_alignment <= kPortAlignment) {
        return allocate(_size);
    }

    constexpr auto maxSize = static_cast<std::size_t>(-1);
    if (_size > maxSize - sizeof(void *) - (_alignment - 1U)) {
        return nullptr;
    }

    auto *raw = static_cast<unsigned char *>(
        allocate(_size + sizeof(void *) + _alignment - 1U));
    if (raw == nullptr) {
        return nullptr;
    }

    const auto rawAddress = reinterpret_cast<std::uintptr_t>(raw);
    const auto alignedAddress =
        (rawAddress + sizeof(void *) + _alignment - 1U) &
        ~(static_cast<std::uintptr_t>(_alignment) - 1U);
    auto *result = reinterpret_cast<void *>(alignedAddress);
    reinterpret_cast<void **>(result)[-1] = raw;
    return result;
}

void freeAligned(void *_ptr, std::size_t _alignment) noexcept
{
    if (_ptr == nullptr) {
        return;
    }
    if (_alignment <= kPortAlignment) {
        vPortFree(_ptr);
        return;
    }

    auto *header = reinterpret_cast<void **>(
        static_cast<unsigned char *>(_ptr) - sizeof(void *));
    vPortFree(header[0]);
}

void *allocateOrStop(std::size_t _size) noexcept
{
    void *result = allocate(_size);
    if (result == nullptr) {
        allocationFailed();
    }
    return result;
}

void *allocateAlignedOrStop(std::size_t _size,
                            std::size_t _alignment) noexcept
{
    void *result = allocateAligned(_size, _alignment);
    if (result == nullptr) {
        allocationFailed();
    }
    return result;
}
} // namespace

void *operator new(std::size_t _size)
{
    return allocateOrStop(_size);
}

void *operator new[](std::size_t _size)
{
    return allocateOrStop(_size);
}

void *operator new(std::size_t _size, std::align_val_t _alignment)
{
    return allocateAlignedOrStop(_size,
                                 static_cast<std::size_t>(_alignment));
}

void *operator new[](std::size_t _size, std::align_val_t _alignment)
{
    return allocateAlignedOrStop(_size,
                                 static_cast<std::size_t>(_alignment));
}

void *operator new(std::size_t _size, const std::nothrow_t &) noexcept
{
    return allocate(_size);
}

void *operator new[](std::size_t _size, const std::nothrow_t &) noexcept
{
    return allocate(_size);
}

void *operator new(std::size_t _size, std::align_val_t _alignment,
                   const std::nothrow_t &) noexcept
{
    return allocateAligned(_size, static_cast<std::size_t>(_alignment));
}

void *operator new[](std::size_t _size, std::align_val_t _alignment,
                     const std::nothrow_t &) noexcept
{
    return allocateAligned(_size, static_cast<std::size_t>(_alignment));
}

void operator delete(void *_ptr) noexcept
{
    vPortFree(_ptr);
}

void operator delete[](void *_ptr) noexcept
{
    vPortFree(_ptr);
}

void operator delete(void *_ptr, std::align_val_t _alignment) noexcept
{
    freeAligned(_ptr, static_cast<std::size_t>(_alignment));
}

void operator delete[](void *_ptr, std::align_val_t _alignment) noexcept
{
    freeAligned(_ptr, static_cast<std::size_t>(_alignment));
}

void operator delete(void *_ptr, std::size_t) noexcept
{
    vPortFree(_ptr);
}

void operator delete[](void *_ptr, std::size_t) noexcept
{
    vPortFree(_ptr);
}

void operator delete(void *_ptr, std::size_t,
                     std::align_val_t _alignment) noexcept
{
    freeAligned(_ptr, static_cast<std::size_t>(_alignment));
}

void operator delete[](void *_ptr, std::size_t,
                       std::align_val_t _alignment) noexcept
{
    freeAligned(_ptr, static_cast<std::size_t>(_alignment));
}
