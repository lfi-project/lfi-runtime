// Shared pointer write/readback tests.
// Assumes: sandbox, TEST/PASS macros, test counters in scope.
// Uses copy_to/copy_from for backend portability.

{

    TEST("write_int + read_int");
    auto ip = sandbox.template alloc<int>(1);
    sandbox.call<void(int*, int)>("write_int", ip, 42);
    int readback = sandbox.call<int(int*)>("read_int", ip);
    assert(readback == 42);
    PASS();

    TEST("write_int overwrite");
    sandbox.call<void(int*, int)>("write_int", ip, 99);
    readback = sandbox.call<int(int*)>("read_int", ip);
    assert(readback == 99);
    sandbox.free(ip);
    PASS();

    TEST("swap_ints");
    auto a = sandbox.template alloc<int>(1);
    auto b = sandbox.template alloc<int>(1);
    sandbox.call<void(int*, int)>("write_int", a, 100);
    sandbox.call<void(int*, int)>("write_int", b, 200);
    sandbox.call<void(int*, int*)>("swap_ints", a, b);
    assert(sandbox.call<int(int*)>("read_int", a) == 200);
    assert(sandbox.call<int(int*)>("read_int", b) == 100);
    sandbox.free(a);
    sandbox.free(b);
    PASS();
}
