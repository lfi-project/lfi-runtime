// Shared CallContext tests.
// Assumes: sandbox, TEST/PASS macros, test counters in scope.

{

    TEST("context: in, out and inout");
    {
        auto ctx = sandbox.context();
        int a = 1, b = 2, v = 0, w = 41, kept = 123;
        sandbox.call(ctx, SBOX_FN(swap_ints), ctx.inout(a), ctx.inout(b));
        sandbox.call(ctx, SBOX_FN(write_int), ctx.out(v), 7);
        assert(sandbox.call(ctx, SBOX_FN(read_const_int), ctx.in(w)) == 41);
        sandbox.call<void(int*, int)>(ctx, "maybe_write_int", ctx.out(kept),
                                      -1);
        assert(a == 2 && b == 1 && v == 7 && kept == 123);
    }
    PASS();

    TEST("context: values are copied back once, after the call that takes them");
    {
        // The host storage is reused after each call, so a later copy-back,
        // from another call or from the destructor, would overwrite it.
        int slots[3] = {-1, -1, -1};
        int prepared = 0;
        {
            auto ctx = sandbox.context();
            auto op = ctx.out(prepared);
            for (int i = 0; i < 3; i++) {
                sbox::sbox<int*> p = ctx.out(slots[i]);
                sandbox.call(ctx, SBOX_FN(write_int), p, i);
                assert(slots[i] == i && prepared == 0);
            }
            for (int i = 0; i < 3; i++)
                slots[i] = 100;
            sandbox.call(ctx, SBOX_FN(write_int), op, 5);
            assert(prepared == 5);
        }
        for (int i = 0; i < 3; i++)
            assert(slots[i] == 100);
    }
    PASS();

    TEST("context: call with an older context while a newer one is live");
    {
        auto c1 = sandbox.context();
        int a = 0, b = 0;
        auto oa = c1.out(a);
        {
            auto c2 = sandbox.context();
            sandbox.call<void(int*, int)>(c2, "write_int", c2.out(b), 2);
            sandbox.call<void(int*, int)>(c1, "write_int", oa, 1);
        }
        assert(a == 1 && b == 2);
    }
    PASS();

    TEST("context: call that also passes a callback");
    {
        auto ctx = sandbox.context();
        auto cb = sandbox.register_callback(context_add_cb);
        assert(sandbox.call(ctx, SBOX_FN(apply_binary_callback), cb, 1, 2) ==
               3);
    }
    PASS();

    TEST("context: scoped enum return and out value");
    {
        enum class Code : int { Zero = 0 };
        assert(sandbox.call<Code(int)>("negate", -3) == static_cast<Code>(3));
        auto ctx = sandbox.context();
        Code c = Code::Zero;
        sandbox.call<void(Code*, int)>(ctx, "write_int", ctx.out(c), 9);
        assert(c == static_cast<Code>(9));
    }
    PASS();

    TEST("context: pointer out parameter stays tainted");
    {
        auto ctx = sandbox.context();
        sbox::sbox<const char*> name;
        sandbox.call(ctx, SBOX_FN(get_version_ptr), ctx.out(name));
        assert(sandbox.read_string(name, 64) == "testlib 1.0");
    }
    PASS();
}
