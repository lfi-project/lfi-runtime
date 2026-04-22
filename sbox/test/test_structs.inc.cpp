// Shared struct tests (flat structs, structs with pointer fields).
// Assumes: sandbox, TEST/PASS macros, test counters, Point/Complex/NamedArray
// in scope. Uses copy_to/copy_from/copy_string for backend portability.

{

    TEST("point_init + point_sum");
    auto p = sandbox.template alloc<Point>(1);
    sandbox.call<void(Point*, int, int)>("point_init", p, 10, 20);
    Point host_p;
    sandbox.copy_from(&host_p, p, sizeof(Point));
    assert(host_p.x == 10);
    assert(host_p.y == 20);
    int ps = sandbox.call<int(Point*)>("point_sum", p);
    assert(ps == 30);
    PASS();

    TEST("point_scale");
    sandbox.call<void(Point*, int)>("point_scale", p, 3);
    sandbox.copy_from(&host_p, p, sizeof(Point));
    assert(host_p.x == 30);
    assert(host_p.y == 60);
    ps = sandbox.call<int(Point*)>("point_sum", p);
    assert(ps == 90);
    sandbox.free(p);
    PASS();

    TEST("complex_magnitude_sq");
    auto c = sandbox.template alloc<Complex>(1);
    Complex host_c = {3.0, 4.0};
    sandbox.copy_to(c, &host_c, sizeof(Complex));
    double dr = sandbox.call<double(Complex*)>("complex_magnitude_sq", c);
    assert(fabs(dr - 25.0) < 1e-9);
    sandbox.free(c);
    PASS();


    TEST("named_array_init + named_array_sum");
    // Allocate the struct, its name string, and its values array in sandbox
    auto na = sandbox.template alloc<NamedArray>(1);
    auto na_name = sandbox.copy_string("test_array");
    auto na_vals = sandbox.template alloc<int>(4);
    int host_vals[4] = {10, 20, 30, 40};
    sandbox.copy_to(na_vals, host_vals, sizeof(int) * 4);
    // Initialize the struct (sets pointer fields inside sandbox)
    sandbox.call<void(NamedArray*, char*, int*, int)>("named_array_init", na,
                                                      na_name, na_vals, 4);
    // Verify the sandbox can traverse the pointer fields
    int na_total = sandbox.call<int(NamedArray*)>("named_array_sum", na);
    assert(na_total == 100);
    PASS();

    TEST("named_array_name_len");
    int na_len = sandbox.call<int(NamedArray*)>("named_array_name_len", na);
    assert(na_len == 10);  // strlen("test_array")
    sandbox.free(na_vals);
    sandbox.free(na_name);
    sandbox.free(na);
    PASS();
}
