#include "sbox/lfi.h"
#include "test_helpers.h"

#include <cerrno>

int main() {
    auto sb = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_memory.inc.cpp"

    TEST("mmap refuses executable mappings");
    {
        errno = 0;
        void* p = sandbox.mmap(nullptr, 4096, PROT_READ | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(p == MAP_FAILED && errno == EINVAL);
    }
    PASS();

    TEST("mmap with MAP_FIXED_NOREPLACE does not replace a mapping");
    {
        void* p = sandbox.mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(p != MAP_FAILED);
        void* q = sandbox.mmap(p, 4096, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS |
                                   MAP_FIXED_NOREPLACE,
                               -1, 0);
        assert(q == MAP_FAILED && errno == EEXIST);
        assert(sandbox.munmap(p, 4096) == 0);
    }
    PASS();

    TEST_SUMMARY();
}
