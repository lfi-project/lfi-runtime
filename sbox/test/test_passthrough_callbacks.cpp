#include "sbox/passthrough.h"

using SboxType = sbox::Passthrough;

#include "test_callbacks.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<SboxType> sandbox(TESTLIB);
#include "test_callbacks.inc.cpp"
    TEST_SUMMARY();
}
