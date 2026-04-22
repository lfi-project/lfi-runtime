// Shared callback tests.
// Assumes: sandbox, TEST/PASS macros, test counters, callback functions in
// scope.

{

    TEST("basic callback (void(int))");
    callback_value = 0;
    auto cb = sandbox.register_callback(my_callback);
    assert(cb != nullptr);
    sandbox.call<void(void (*)(int))>("set_callback", cb);
    sandbox.call<void(int)>("trigger_callback", 42);
    assert(callback_value == 42);
    PASS();

    TEST("callback re-trigger");
    callback_value = 0;
    sandbox.call<void(int)>("trigger_callback", 999);
    assert(callback_value == 999);
    PASS();

    TEST("binary callback (int(int,int)) - add");
    auto add_cb = sandbox.register_callback(my_add_callback);
    assert(add_cb != nullptr);
    int cbr = sandbox.call<int(int (*)(int, int), int, int)>(
        "apply_binary_callback", add_cb, 10, 20);
    assert(cbr == 30);
    PASS();

    TEST("binary callback (int(int,int)) - multiply");
    auto mul_cb = sandbox.register_callback(my_multiply_callback);
    assert(mul_cb != nullptr);
    cbr = sandbox.call<int(int (*)(int, int), int, int)>(
        "apply_binary_callback", mul_cb, 6, 7);
    assert(cbr == 42);
    PASS();

    TEST("double callback (double(double)) - double");
    auto dbl_cb = sandbox.register_callback(my_double_callback);
    assert(dbl_cb != nullptr);
    double dr = sandbox.call<double(double (*)(double), double)>(
        "apply_double_callback", dbl_cb, 3.14);
    assert(fabs(dr - 6.28) < 1e-9);
    PASS();

    TEST("double callback (double(double)) - square");
    auto sq_cb = sandbox.register_callback(my_square_callback);
    assert(sq_cb != nullptr);
    dr = sandbox.call<double(double (*)(double), double)>(
        "apply_double_callback", sq_cb, 5.0);
    assert(fabs(dr - 25.0) < 1e-9);
    PASS();

    TEST("quad callback (int(int,int,int,int))");
    auto quad_cb = sandbox.register_callback(my_quad_callback);
    assert(quad_cb != nullptr);
    cbr = sandbox.call<int(int (*)(int, int, int, int), int, int, int, int)>(
        "apply_quad_callback", quad_cb, 3, 4, 5, 6);
    assert(cbr == 42);  // 3*4 + 5*6 = 12 + 30
    PASS();

    // TODO: LFI callback trampoline doesn't forward stack args from sandbox
    // to host, so callbacks with >6 int args (x86-64) or >8 (aarch64) fail.
#ifndef SBOX_SKIP_STACK_ARG_CALLBACKS
    TEST("8-arg callback (int(int,int,int,int,int,int,int,int))");
    auto sum8_cb = sandbox.register_callback(my_sum8_callback);
    assert(sum8_cb != nullptr);
    cbr = sandbox.call<int(int (*)(int, int, int, int, int, int, int, int),
                           int, int, int, int, int, int, int, int)>(
        "apply_callback8", sum8_cb, 1, 2, 3, 4, 5, 6, 7, 8);
    assert(cbr == 36);
    PASS();

    TEST("9-arg callback (int(int,...,int) x9)");
    auto sum9_cb = sandbox.register_callback(my_sum9_callback);
    assert(sum9_cb != nullptr);
    cbr = sandbox.call<int(int (*)(int, int, int, int, int, int, int, int, int),
                           int, int, int, int, int, int, int, int, int)>(
        "apply_callback9", sum9_cb, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    assert(cbr == 45);
    PASS();
#endif

    TEST("pointer callback (sbox<int*> arg)");
    auto cb_arr = sandbox.template idmem_alloc<int>(4);
    int cb_vals[4] = {10, 20, 30, 40};
    sandbox.copy_to(cb_arr, cb_vals, sizeof(int) * 4);
    auto ptr_cb = sandbox.template register_callback<my_ptr_sum_callback>();
    assert(ptr_cb != nullptr);
    cbr = sandbox.call<int(int (*)(int*, int), int*, int)>(
        "apply_ptr_callback", ptr_cb, cb_arr, 4);
    assert(cbr == 100);
    sandbox.idmem_reset();
    PASS();

    TEST("double pointer callback (sbox<double*> arg)");
    {
        auto darr = sandbox.template idmem_alloc<double>(3);
        double dvals[3] = {1.5, 2.5, 3.0};
        sandbox.copy_to(darr, dvals, sizeof(double) * 3);
        auto dptr_cb =
            sandbox.template register_callback<my_double_ptr_sum_callback>();
        assert(dptr_cb != nullptr);
        double dsum =
            sandbox.call<double(double (*)(double*, int), double*, int)>(
                "apply_double_ptr_callback", dptr_cb, darr, 3);
        assert(fabs(dsum - 7.0) < 1e-9);
        sandbox.idmem_reset();
    }
    PASS();

    TEST("mutating pointer callback (sbox<int*> write)");
    {
        auto marr = sandbox.template idmem_alloc<int>(3);
        int mvals[3] = {5, 10, 15};
        sandbox.copy_to(marr, mvals, sizeof(int) * 3);
        auto mut_cb =
            sandbox.template register_callback<my_mutate_callback>();
        assert(mut_cb != nullptr);
        sandbox.call<void(void (*)(int*, int), int*, int)>(
            "apply_mutate_callback", mut_cb, marr, 3);
        // Verify the callback doubled each element
        int result[3];
        sandbox.copy_from(result, marr, sizeof(int) * 3);
        assert(result[0] == 10);
        assert(result[1] == 20);
        assert(result[2] == 30);
        sandbox.idmem_reset();
    }
    PASS();
}
