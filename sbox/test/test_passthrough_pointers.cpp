#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox(TESTLIB);
#include "test_pointers.inc.cpp"
#include "test_context.inc.cpp"
    TEST_SUMMARY();
}
