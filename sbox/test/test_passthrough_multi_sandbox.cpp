#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sb1(TESTLIB);
    sbox::Sandbox<sbox::Passthrough> sb2(TESTLIB2);
#include "test_multi_sandbox.inc.cpp"
    TEST_SUMMARY();
}
