#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
constexpr std::size_t kMaxBlocks = 64;
constexpr std::size_t kAllocHeader = 16;

struct Block_s {
    void *ptr;
};

Block_s gBlocks[kMaxBlocks]{};
std::size_t gBlockCount = 0;
std::size_t gForeignFrees = 0;
bool gFailNext = false;
std::jmp_buf *gOomJump = nullptr;

bool removeBlock(void *_ptr)
{
    for (std::size_t i = 0; i < gBlockCount; ++i) {
        if (gBlocks[i].ptr == _ptr) {
            gBlocks[i] = gBlocks[gBlockCount - 1];
            --gBlockCount;
            return true;
        }
    }
    return false;
}

int gFailures = 0;
#define CHECK(_condition)                                                        \
    do {                                                                         \
        if (!(_condition)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #_condition);   \
            ++gFailures;                                                         \
        }                                                                        \
    } while (false)

void expectAllocated(const void *_ptr, std::size_t _alignment)
{
    CHECK(_ptr != nullptr);
    CHECK(reinterpret_cast<std::uintptr_t>(_ptr) % _alignment == 0);
}
} // namespace

extern "C" void *pvPortMalloc(std::size_t _size)
{
    if (gFailNext) {
        gFailNext = false;
        return nullptr;
    }
    if (gBlockCount == kMaxBlocks) {
        return nullptr;
    }

    auto *raw = static_cast<unsigned char *>(
        std::malloc(_size + kAllocHeader));
    if (raw == nullptr) {
        return nullptr;
    }
    void *ptr = raw + kAllocHeader;
    gBlocks[gBlockCount++].ptr = ptr;
    return ptr;
}


// MinGW has no aligned_alloc. Give the legacy code a libc-side allocation so
// the cross-heap vPortFree regression is observable on the host.
extern "C" void *aligned_alloc(std::size_t _alignment, std::size_t _size)
{
    auto *raw = static_cast<unsigned char *>(
        std::malloc(_size + _alignment));
    if (raw == nullptr) {
        return nullptr;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(raw);
    return reinterpret_cast<void *>(
        (address + _alignment - 1U) & ~(_alignment - 1U));
}
extern "C" void vPortFree(void *_ptr)
{
    if (_ptr == nullptr) {
        return;
    }
    if (!removeBlock(_ptr)) {
        ++gForeignFrees;
        return;
    }
    std::free(static_cast<unsigned char *>(_ptr) - kAllocHeader);
}

void testDisableInterruptsForOom()
{
    if (gOomJump != nullptr) {
        std::longjmp(*gOomJump, 1);
    }
    std::fprintf(stderr, "unexpected allocation fail-stop\n");
    std::exit(3);
}

bool gThrowingReturned = false;

void testThrowingOom(bool _aligned)
{
    std::jmp_buf jump{};
    gThrowingReturned = false;
    gOomJump = &jump;
    if (setjmp(jump) == 0) {
        gFailNext = true;
        if (_aligned) {
            (void)::operator new(16, std::align_val_t{32});
        } else {
            (void)::operator new(16);
        }
        gThrowingReturned = true;
    }
    gOomJump = nullptr;
    gFailNext = false;
    CHECK(!gThrowingReturned);
}

int main()
{
    CHECK(gBlockCount == 0);
    CHECK(gForeignFrees == 0);

    void *plain = ::operator new(24);
    CHECK(plain != nullptr);
    ::operator delete(plain);

    void *zero = ::operator new(0);
    CHECK(zero != nullptr);
    ::operator delete(zero);

    void *plainArray = ::operator new[](24);
    CHECK(plainArray != nullptr);
    ::operator delete[](plainArray);

    void *aligned8 = ::operator new(8, std::align_val_t{8});
    expectAllocated(aligned8, 8);
    ::operator delete(aligned8, std::align_val_t{8});

    void *aligned16 = ::operator new(1, std::align_val_t{16});
    expectAllocated(aligned16, 16);
    ::operator delete(aligned16, std::align_val_t{16});

    void *aligned32 = ::operator new(64, std::align_val_t{32});
    expectAllocated(aligned32, 32);
    ::operator delete(aligned32, 64, std::align_val_t{32});

    void *aligned64 = ::operator new[](64, std::align_val_t{64});
    expectAllocated(aligned64, 64);
    ::operator delete[](aligned64, 64, std::align_val_t{64});

    CHECK(gForeignFrees == 0);
    CHECK(gBlockCount == 0);

    gFailNext = true;
    void *nothrowPlain = ::operator new(16, std::nothrow);
    CHECK(nothrowPlain == nullptr);

    gFailNext = true;
    void *nothrowAligned =
        ::operator new(64, std::align_val_t{32}, std::nothrow);
    CHECK(nothrowAligned == nullptr);

    (void)::operator new(static_cast<std::size_t>(-1),
                         std::align_val_t{32}, std::nothrow);
    CHECK(gBlockCount == 0);

    testThrowingOom(false);
    testThrowingOom(true);

    CHECK(gForeignFrees == 0);
    CHECK(gBlockCount == 0);

    if (gFailures == 0) {
        std::puts("runtime support host test: PASS");
        return 0;
    }
    std::printf("runtime support host test: %d failure(s)\n", gFailures);
    return 1;
}