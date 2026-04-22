// Shared multi-sandbox tests.
// Assumes: sb1, sb2, TEST/PASS macros in scope.
// sb1 loads testlib (has add, fill_ints, etc.)
// sb2 loads testlib2 (has multiply, square, fill_ints)

{

    TEST("call different sandboxes");
    int sum = sb1.call<int(int, int)>("add", 10, 20);
    int prod = sb2.call<int(int, int)>("multiply", 10, 20);
    assert(sum == 30);
    assert(prod == 200);
    PASS();

    TEST("interleaved calls");
    for (int i = 0; i < 100; i++) {
        assert(sb1.call<int(int, int)>("add", i, 1) == i + 1);
        assert(sb2.call<int(int)>("square", i) == i * i);
    }
    PASS();

    TEST("independent memory");
    auto buf1 = sb1.template alloc<int>(4);
    auto buf2 = sb2.template alloc<int>(4);
    sb1.call<void(int*, int, int)>("fill_ints", buf1, 4, 0);
    sb2.call<void(int*, int, int)>("fill_ints", buf2, 4, 100);
    int host1[4], host2[4];
    sb1.copy_from(host1, buf1, sizeof(int) * 4);
    sb2.copy_from(host2, buf2, sizeof(int) * 4);
    for (int i = 0; i < 4; i++) {
        assert(host1[i] == i);
        assert(host2[i] == 100 + i);
    }
    sb1.free(buf1);
    sb2.free(buf2);
    PASS();

}
