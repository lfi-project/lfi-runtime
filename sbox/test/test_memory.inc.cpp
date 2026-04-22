// Shared memory tests (arrays, calloc/realloc, alloc/free stress).
// Assumes: sandbox, TEST/PASS macros, test counters in scope.
// Uses copy_to/copy_from for backend portability.

{

    TEST("fill_ints + sum_ints");
    auto arr = sandbox.template alloc<int>(10);
    sandbox.call<void(int*, int, int)>("fill_ints", arr, 10, 0);
    int host_arr[10];
    sandbox.copy_from(host_arr, arr, sizeof(int) * 10);
    for (int i = 0; i < 10; i++) {
        assert(host_arr[i] == i);
    }
    int total = sandbox.call<int(int*, int)>("sum_ints", arr, 10);
    assert(total == 45);  // 0+1+...+9
    sandbox.free(arr);
    PASS();

    TEST("fill_ints with offset");
    arr = sandbox.template alloc<int>(5);
    sandbox.call<void(int*, int, int)>("fill_ints", arr, 5, 100);
    total = sandbox.call<int(int*, int)>("sum_ints", arr, 5);
    assert(total == 510);  // 100+101+102+103+104
    sandbox.free(arr);
    PASS();


    TEST("calloc zeroes memory");
    auto zarr = sandbox.template calloc<int>(4);
    int host_zarr[4];
    sandbox.copy_from(host_zarr, zarr, sizeof(int) * 4);
    for (int i = 0; i < 4; i++) {
        assert(host_zarr[i] == 0);
    }
    sandbox.free(zarr);
    PASS();

    TEST("realloc preserves data");
    arr = sandbox.template alloc<int>(2);
    int init[2] = {11, 22};
    sandbox.copy_to(arr, init, sizeof(int) * 2);
    arr = sandbox.template realloc<int>(arr, 4);
    int host_realloc[2];
    sandbox.copy_from(host_realloc, arr, sizeof(int) * 2);
    assert(host_realloc[0] == 11);
    assert(host_realloc[1] == 22);
    sandbox.free(arr);
    PASS();


    TEST("repeated alloc/free (100 cycles)");
    for (int i = 0; i < 100; i++) {
        auto tmp = sandbox.template alloc<int>(16);
        sandbox.call<void(int*, int, int)>("fill_ints", tmp, 16, i);
        int s = sandbox.call<int(int*, int)>("sum_ints", tmp, 16);
        // sum of [i, i+1, ..., i+15] = 16*i + 120
        assert(s == 16 * i + 120);
        sandbox.free(tmp);
    }
    PASS();
}
