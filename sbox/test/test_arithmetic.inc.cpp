// Shared arithmetic tests (int, double, float, long long, unsigned, many/max
// params). Assumes: sandbox, TEST/PASS macros, test counters in scope.

{

    TEST("add(10, 32) == 42");
    assert(sandbox.call<int(int, int)>("add", 10, 32) == 42);
    PASS();

    TEST("multiply(6, 7) == 42");
    assert(sandbox.call<int(int, int)>("multiply", 6, 7) == 42);
    PASS();

    TEST("add(0, 0) == 0");
    assert(sandbox.call<int(int, int)>("add", 0, 0) == 0);
    PASS();

    TEST("add(-10, 10) == 0");
    assert(sandbox.call<int(int, int)>("add", -10, 10) == 0);
    PASS();

    TEST("negate(42) == -42");
    assert(sandbox.call<int(int)>("negate", 42) == -42);
    PASS();

    TEST("negate(-1) == 1");
    assert(sandbox.call<int(int)>("negate", -1) == 1);
    PASS();

    TEST("negate(0) == 0");
    assert(sandbox.call<int(int)>("negate", 0) == 0);
    PASS();


    TEST("add_double(1.5, 2.5) == 4.0");
    double dr = sandbox.call<double(double, double)>("add_double", 1.5, 2.5);
    assert(fabs(dr - 4.0) < 1e-9);
    PASS();

    TEST("add_double(-1.0, 1.0) == 0.0");
    dr = sandbox.call<double(double, double)>("add_double", -1.0, 1.0);
    assert(fabs(dr) < 1e-9);
    PASS();

    TEST("add_double with large values");
    dr = sandbox.call<double(double, double)>("add_double", 1e15, 1e15);
    assert(fabs(dr - 2e15) < 1e6);
    PASS();


    TEST("multiply_float(3.0f, 4.0f) == 12.0f");
    float fr = sandbox.call<float(float, float)>("multiply_float", 3.0f, 4.0f);
    assert(fabsf(fr - 12.0f) < 1e-5f);
    PASS();

    TEST("multiply_float(0.5f, 0.5f) == 0.25f");
    fr = sandbox.call<float(float, float)>("multiply_float", 0.5f, 0.5f);
    assert(fabsf(fr - 0.25f) < 1e-5f);
    PASS();


    TEST("add_long_long with large values");
    long long llr = sandbox.call<long long(long long, long long)>(
        "add_long_long", 1LL << 40, 1LL << 40);
    assert(llr == (1LL << 41));
    PASS();

    TEST("add_long_long(-1, 1) == 0");
    llr = sandbox.call<long long(long long, long long)>("add_long_long", -1LL,
                                                        1LL);
    assert(llr == 0);
    PASS();


    TEST("add_unsigned(100, 200) == 300");
    unsigned int ur = sandbox.call<unsigned int(unsigned int, unsigned int)>(
        "add_unsigned", 100u, 200u);
    assert(ur == 300u);
    PASS();

    TEST("add_unsigned with large values");
    ur = sandbox.call<unsigned int(unsigned int, unsigned int)>(
        "add_unsigned", 0xFFFFFF00u, 0x100u);
    assert(ur == 0u);  // wraps around
    PASS();


    TEST("sum6(1,2,3,4,5,6) == 21");
    int s6 = sandbox.call<int(int, int, int, int, int, int)>("sum6", 1, 2, 3, 4,
                                                             5, 6);
    assert(s6 == 21);
    PASS();

    TEST("weighted_sum(1,2,3, 0.5,0.3,0.2) == 1.7");
    dr = sandbox.call<double(double, double, double, double, double, double)>(
        "weighted_sum", 1.0, 2.0, 3.0, 0.5, 0.3, 0.2);
    assert(fabs(dr - 1.7) < 1e-9);
    PASS();


    TEST("sum8(1..8) == 36");
    int s8 = sandbox.call<int(int, int, int, int, int, int, int, int)>(
        "sum8", 1, 2, 3, 4, 5, 6, 7, 8);
    assert(s8 == 36);
    PASS();

    TEST("sum8_double(1.0..8.0) == 36.0");
    dr = sandbox.call<double(double, double, double, double, double, double,
                             double, double)>("sum8_double", 1.0, 2.0, 3.0, 4.0,
                                              5.0, 6.0, 7.0, 8.0);
    assert(fabs(dr - 36.0) < 1e-9);
    PASS();

    TEST("sum9(1..9) == 45");
    int s9 =
        sandbox.call<int(int, int, int, int, int, int, int, int, int)>(
            "sum9", 1, 2, 3, 4, 5, 6, 7, 8, 9);
    assert(s9 == 45);
    PASS();

    TEST("sum10(1..10) == 55");
    int s10 =
        sandbox.call<int(int, int, int, int, int, int, int, int, int, int)>(
            "sum10", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
    assert(s10 == 55);
    PASS();

    TEST("sum10_double(1.0..10.0) == 55.0");
    dr = sandbox.call<double(double, double, double, double, double, double,
                             double, double, double, double)>(
        "sum10_double", 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0);
    assert(fabs(dr - 55.0) < 1e-9);
    PASS();

    TEST("mixed10(1,2.0,3,4.0,5,6.0,7,8.0,9,10.0) == 55.0");
    dr = sandbox.call<double(int, double, int, double, int, double, int, double,
                             int, double)>(
        "mixed10", 1, 2.0, 3, 4.0, 5, 6.0, 7, 8.0, 9, 10.0);
    assert(fabs(dr - 55.0) < 1e-9);
    PASS();
}
