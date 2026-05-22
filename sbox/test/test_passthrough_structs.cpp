#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox(TESTLIB);
#include "test_structs.inc.cpp"
    TEST_SUMMARY();
}
