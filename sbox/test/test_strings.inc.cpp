// Shared string tests.
// Assumes: sandbox, TEST/PASS macros, test counters in scope.
// Uses copy_to/copy_from/copy_string for backend portability.

{

    TEST("process_string echo");
    auto buf = sandbox.copy_string("Hello, sandbox!");
    sbox::sbox<char*> echoed = sandbox.call<char*(char*)>("process_string", buf);
    char host_buf[256];
    sandbox.copy_from(host_buf, echoed, 16);
    assert(strcmp(host_buf, "Hello, sandbox!") == 0);
    PASS();

    TEST("string_length");
    int len = sandbox.call<int(const char*)>("string_length", buf);
    assert(len == 15);
    PASS();

    TEST("string_to_upper");
    sandbox.call<void(char*)>("string_to_upper", buf);
    sandbox.copy_from(host_buf, buf, 16);
    assert(strcmp(host_buf, "HELLO, SANDBOX!") == 0);
    sandbox.free(buf);
    PASS();

    TEST("string_length empty string");
    buf = sandbox.copy_string("");
    len = sandbox.call<int(const char*)>("string_length", buf);
    assert(len == 0);
    sandbox.free(buf);
    PASS();

    TEST("copy_string round-trip");
    auto str = sandbox.copy_string("test string");
    sbox::sbox<char*> echoed2 = sandbox.call<char*(char*)>("process_string", str);
    char result_buf[256];
    sandbox.copy_from(result_buf, echoed2, 12);
    assert(strcmp(result_buf, "test string") == 0);
    sandbox.free(str);
    PASS();

    TEST("read_string returns content for terminated string");
    {
        auto rs = sandbox.copy_string("read me");
        std::string host = sandbox.read_string(rs, 64);
        assert(host == "read me");
        sandbox.free(rs);
    }
    PASS();

    TEST("read_string returns empty for unterminated buffer");
    {
        auto rs = sandbox.template alloc<char>(8);
        char raw[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
        sandbox.copy_to(rs, raw, 8);
        std::string host = sandbox.read_string(rs, 8);
        assert(host.empty());
        sandbox.free(rs);
    }
    PASS();

    TEST("read_string returns empty for null pointer");
    {
        std::string host = sandbox.read_string(sbox::sbox<char*>(), 16);
        assert(host.empty());
    }
    PASS();

    TEST("read_string respects scan limit");
    {
        auto rs = sandbox.copy_string("longer than the limit");
        // Terminator is at index 21 but we only scan 5 bytes.
        std::string host = sandbox.read_string(rs, 5);
        assert(host.empty());
        sandbox.free(rs);
    }
    PASS();
}
