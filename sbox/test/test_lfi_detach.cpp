// Worker-thread detach with sandbox re-creation. A worker thread attaches
// to one sandbox by calling into it, detaches, then the host destroys
// that sandbox and creates a new one in its place. The worker then calls
// into the new sandbox. LFIManager::init(1) makes the sandbox slot
// recycle, so each Sandbox<LFI> must remain a distinct cache key for
// per-thread contexts regardless of which underlying LFIBox address is
// reused.

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
    sb1.reset();

    auto sb2 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb2);
    target.store(sb2.get());

    phase.store(3);
    while (phase.load() < 4) std::this_thread::yield();
    worker.join();

    TEST("worker call into first sandbox");
    assert(phase1_result == 3);
    PASS();

    TEST("worker call into replacement sandbox");
    assert(phase2_result == 30);
    PASS();

    TEST_SUMMARY();
    return 0;
}
