// Shared misc tests (void functions).
// Assumes: sandbox, TEST/PASS macros, test counters in scope.

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
}
