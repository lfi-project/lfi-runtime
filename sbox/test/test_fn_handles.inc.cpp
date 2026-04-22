// Shared function handle tests.
// Assumes: sandbox, TEST/PASS macros, test counters in scope.

{

    TEST("fn handle: add(100, 200) == 300");
    auto add_fn = sandbox.fn<int(int, int)>("add");
    assert(add_fn(100, 200) == 300);
    PASS();

    TEST("fn handle: repeated calls");
    for (int i = 0; i < 100; i++) {
        assert(add_fn(i, i) == 2 * i);
    }
    PASS();

    TEST("fn handle: add_double(1.1, 2.2)");
    auto add_double_fn = sandbox.fn<double(double, double)>("add_double");
    double dr = add_double_fn(1.1, 2.2);
    assert(fabs(dr - 3.3) < 1e-9);
    PASS();
}
