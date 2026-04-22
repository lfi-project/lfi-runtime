#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sbox/lfi.h"
#include "test_helpers.h"

int main() {
    auto sb = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_misc.inc.cpp"
    TEST_SUMMARY();
}
