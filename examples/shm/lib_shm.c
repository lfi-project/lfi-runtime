#include <stddef.h>
#include <string.h>

void fill_buffer(void* buf, size_t len, unsigned char value) {
    memset(buf, value, len);
}

void increment_buffer(unsigned char* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i]++;
    }
}
