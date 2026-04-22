#define SBOX_SKIP_STACK_ARG_CALLBACKS
#include "sbox/lfi.h"

using SboxType = sbox::LFI;

#include "test_callbacks.h"
#include "test_helpers.h"

int main() {
    auto sb = sbox::Sandbox<SboxType>::create("./testlib.lfi");
    assert(sb);
    auto& sandbox = *sb;
#include "test_callbacks.inc.cpp"
    TEST_SUMMARY();
}
