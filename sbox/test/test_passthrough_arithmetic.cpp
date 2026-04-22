#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libtestlib.so");
#include "test_arithmetic.inc.cpp"
    TEST_SUMMARY();
}
