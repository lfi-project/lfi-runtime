// Shared misc tests (void functions, error handling).
// Assumes: sandbox, TEST/PASS macros, test counters in scope.
// Requires: <csetjmp>, <csignal>

{

    TEST("noop() + was_noop_called()");
    sandbox.call<void()>("noop");
    int called = sandbox.call<int()>("was_noop_called");
    assert(called == 1);
    PASS();

    TEST("was_noop_called resets");
    called = sandbox.call<int()>("was_noop_called");
    assert(called == 0);
    PASS();


    TEST("invalid symbol aborts");
    {
        // Catch SIGABRT in-process via siglongjmp so the test does not
        // need to fork (which interleaves child output with the parent's
        // TAP stream, particularly under sanitizers).
        static sigjmp_buf abort_jmp;
        static volatile sig_atomic_t caught_abort;
        caught_abort = 0;
        struct sigaction old;
        struct sigaction sa = {};
        sa.sa_handler = [](int) {
            caught_abort = 1;
            siglongjmp(abort_jmp, 1);
        };
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_NODEFER;
        sigaction(SIGABRT, &sa, &old);
        if (sigsetjmp(abort_jmp, 1) == 0) {
            sandbox.call<int()>("nonexistent_function_xyz");
        }
        sigaction(SIGABRT, &old, nullptr);
        assert(caught_abort);
    }
    PASS();
}
