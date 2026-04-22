#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sb1("./libtestlib.so");
    sbox::Sandbox<sbox::Passthrough> sb2("./libtestlib2.so");
#include "test_multi_sandbox.inc.cpp"
    TEST_SUMMARY();
}
