// Shared misc tests (void functions, error handling).
// Assumes: sandbox, TEST/PASS macros, test counters in scope.
// Requires: <sys/wait.h>, <unistd.h>

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
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        // Child: calling invalid symbol should abort
        sandbox.call<int()>("nonexistent_function_xyz");
        _exit(0);  // should not reach here
    }
    int status;
    waitpid(pid, &status, 0);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    PASS();
}
