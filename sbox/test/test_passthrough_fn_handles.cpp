#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libtestlib.so");
#include "test_fn_handles.inc.cpp"
    TEST_SUMMARY();
}
