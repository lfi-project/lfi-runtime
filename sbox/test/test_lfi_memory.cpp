#include "sbox/lfi.h"
#include "test_helpers.h"

#include <cerrno>
#include <unistd.h>

int main() {
    auto sb = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_memory.inc.cpp"

    size_t page = static_cast<size_t>(getpagesize());

    TEST("mmap refuses executable mappings");
    {
        errno = 0;
        void* p = sandbox.mmap(nullptr, page, PROT_READ | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(p == MAP_FAILED && errno == EINVAL);
    }
    PASS();

#ifdef MAP_FIXED_NOREPLACE
    TEST("mmap with MAP_FIXED_NOREPLACE does not replace a mapping");
    {
        void* p = sandbox.mmap(nullptr, page, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(p != MAP_FAILED);
        void* q = sandbox.mmap(p, page, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS |
                                   MAP_FIXED_NOREPLACE,
                               -1, 0);
        assert(q == MAP_FAILED && errno == EEXIST);
        assert(sandbox.munmap(p, page) == 0);
    }
    PASS();
#endif

    TEST_SUMMARY();
}
