#include "sbox/lfi.h"
#include "test_helpers.h"

int main() {
    assert(sbox::LFIManager::init(2));
    auto p1 = sbox::Sandbox<sbox::LFI>::create("./testlib.lfi");
    auto p2 = sbox::Sandbox<sbox::LFI>::create("./testlib2.lfi");
    assert(p1 && p2);
    auto& sb1 = *p1;
    auto& sb2 = *p2;
#include "test_multi_sandbox.inc.cpp"
    TEST_SUMMARY();
}
