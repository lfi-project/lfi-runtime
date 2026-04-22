#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include "sbox/passthrough.h"
#include "test_helpers.h"

int main() {
    sbox::Sandbox<sbox::Passthrough> sandbox("./libtestlib.so");
#include "test_misc.inc.cpp"
    TEST_SUMMARY();
}
