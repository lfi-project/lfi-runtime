// Sandbox re-creation with a recycled sandbox slot. With LFIManager::init(1)
// a new sandbox reuses the previous one's address range once that sandbox's
// memory is released. A worker thread attaches to one sandbox, detaches, and
// then calls into its replacement, which must reuse the range. The main
// thread calls into a sandbox, destroys it, and calls into its replacement
// without detaching, so the per-thread context cache must treat each
// Sandbox<LFI> as distinct even if the new object reuses the old address.

#include "sbox/lfi.h"

#include <atomic>
#include <cstdio>
#include <thread>

#include "test_helpers.h"

int main() {
    if (!sbox::LFIManager::init(1)) {
        fprintf(stderr, "LFIManager::init(1) failed\n");
        return 77;
    }

    std::atomic<int> phase{0};
    std::atomic<sbox::Sandbox<sbox::LFI>*> target{nullptr};

    int phase1_result = 0;
    int phase2_result = 0;

    std::thread worker([&]() {
        while (phase.load() < 1) std::this_thread::yield();
        sbox::Sandbox<sbox::LFI>* sb = target.load();
        phase1_result = sb->call<int(int, int)>("add", 1, 2);
        sb->detach_thread();
        phase.store(2);

        while (phase.load() < 3) std::this_thread::yield();
        sb = target.load();
        phase2_result = sb->call<int(int, int)>("add", 10, 20);
        sb->detach_thread();
        phase.store(4);
    });

    auto sb1 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb1);
    target.store(sb1.get());
    phase.store(1);

    while (phase.load() < 2) std::this_thread::yield();

    target.store(nullptr);
    uintptr_t first_base = lfi_box_info(sb1->native_handle()).base;
    sb1.reset();

    auto sb2 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb2);
    bool reused_range = lfi_box_info(sb2->native_handle()).base == first_base;
    target.store(sb2.get());

    phase.store(3);
    while (phase.load() < 4) std::this_thread::yield();
    worker.join();

    TEST("detached worker released the first sandbox's memory");
    assert(reused_range);
    PASS();

    TEST("main thread call into replacement sandbox");
    {
        sb2.reset();
        auto sb3 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
        assert(sb3);
        assert(sb3->call<int(int, int)>("add", 2, 3) == 5);
        sb3.reset();
        auto sb4 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
        assert(sb4);
        assert(sb4->call<int(int, int)>("add", 4, 5) == 9);
    }
    PASS();

    TEST("worker call into first sandbox");
    assert(phase1_result == 3);
    PASS();

    TEST("worker call into replacement sandbox");
    assert(phase2_result == 30);
    PASS();

    TEST_SUMMARY();
    return 0;
}
