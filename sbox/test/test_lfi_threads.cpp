// Worker threads calling into an LFI sandbox. Workers use a CallContext as
// their first operation, some exit without detaching, and one exits after
// the sandbox has been destroyed. With LFIManager::init(1) there is one
// reserved sandbox slot, so a new sandbox reuses the first one's address
// only if its memory was released when that last worker exited. A thread
// that calls into the sandbox from a thread-local destructor after its
// cleanup has run must not leave a context that a later thread reuses.

#include "sbox/lfi.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

#include "test_helpers.h"

#define NUM_THREADS 4
#define ITERATIONS 200

static sbox::Sandbox<sbox::LFI>* late_sandbox;
static int late_result;

// Constructed before the thread's first sandbox call, so it is destroyed
// after the sandbox's thread cleanup.
struct LateUser {
    ~LateUser() {
        late_result = late_sandbox->call<int(int, int)>("add", 2, 2);
    }
};

int main() {
    if (!sbox::LFIManager::init(1)) {
        fprintf(stderr, "LFIManager::init(1) failed\n");
        return 77;
    }

    auto sb = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb);
    sbox::Sandbox<sbox::LFI>* sandbox = sb.get();

    std::atomic<int> failures{0};
    std::vector<std::thread> workers;
    for (int id = 0; id < NUM_THREADS; id++) {
        workers.emplace_back([&, id]() {
            {
                auto ctx = sandbox->context();
                int v = 0;
                sandbox->call<void(int*, int)>(ctx, "write_int", ctx.out(v),
                                               id);
                if (v != id)
                    failures++;
            }
            for (int i = 0; i < ITERATIONS; i++) {
                if (sandbox->call<int(int, int)>("add", id, i) != id + i)
                    failures++;
                auto p = sandbox->alloc<int>(4);
                if (!p) {
                    failures++;
                    continue;
                }
                p[0] = i;
                if (sandbox->call<int(int*)>("read_int", p) != i)
                    failures++;
                sandbox->free(p);
            }
            if (id % 2 == 0)
                sandbox->detach_thread();
        });
    }
    for (auto& t : workers)
        t.join();

    TEST("concurrent worker calls, allocations, and contexts");
    assert(failures == 0);
    PASS();

    std::atomic<int> phase{0};
    int lingering_result = 0;
    std::thread lingering([&]() {
        lingering_result = sandbox->call<int(int, int)>("add", 1, 2);
        phase.store(1);
        while (phase.load() < 2)
            std::this_thread::yield();
    });
    while (phase.load() < 1)
        std::this_thread::yield();
    uintptr_t first_base = lfi_box_info(sb->native_handle()).base;
    sb.reset();
    phase.store(2);
    lingering.join();

    TEST("worker exits after its sandbox is destroyed");
    assert(lingering_result == 3);
    PASS();

    TEST("sandbox memory is released after the last worker exits");
    auto sb2 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb2);
    assert(lfi_box_info(sb2->native_handle()).base == first_base);
    assert(sb2->call<int(int, int)>("add", 20, 22) == 42);
    PASS();

    TEST("call from a thread-local destructor after cleanup");
    late_sandbox = sb2.get();
    for (int i = 0; i < 4; i++) {
        std::thread t([]() {
            static thread_local LateUser late_user;
            (void) late_user;
            assert(late_sandbox->call<int(int, int)>("add", 1, 1) == 2);
        });
        t.join();
        assert(late_result == 4);
        late_result = 0;
    }
    PASS();

    TEST("LFIManager::destroy refuses while a sandbox is alive");
    assert(!sbox::LFIManager::destroy());
    sb2.reset();
    assert(sbox::LFIManager::destroy());
    PASS();

    TEST_SUMMARY();
    return 0;
}
