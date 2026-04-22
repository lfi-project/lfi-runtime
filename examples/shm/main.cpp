#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#if defined(SBOX_LFI)
#include "sbox/lfi.h"
#else
#include "sbox/passthrough.h"
#endif

#define BUF_SIZE 4096

int main() {
#if defined(SBOX_LFI)
    auto sb = sbox::Sandbox<sbox::LFI>::create("./lib_shm.lfi");
    if (!sb) { fprintf(stderr, "failed to create sandbox\n"); return 1; }
    auto& sandbox = *sb;
#else
    sbox::Sandbox<sbox::Passthrough> sandbox("./libshm.so");
#endif

    // Create shared memory using memfd_create
    int memfd = memfd_create("shared_buffer", 0);
    if (memfd < 0) {
        perror("memfd_create");
        return 1;
    }

    if (ftruncate(memfd, BUF_SIZE) < 0) {
        perror("ftruncate");
        close(memfd);
        return 1;
    }

    // Map the buffer in the host
    unsigned char* host_buf = static_cast<unsigned char*>(::mmap(
        nullptr, BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0));
    if (host_buf == MAP_FAILED) {
        perror("mmap (host)");
        close(memfd);
        return 1;
    }

    // Initialize host buffer to zeros
    memset(host_buf, 0, BUF_SIZE);
    printf("Host buffer initialized to zeros\n");
    printf("First 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n\n",
           host_buf[0], host_buf[1], host_buf[2], host_buf[3], host_buf[4],
           host_buf[5], host_buf[6], host_buf[7]);

    // Map the same buffer in the sandbox
    void* sandbox_buf = sandbox.mmap(nullptr, BUF_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, memfd, 0);
    if (sandbox_buf == MAP_FAILED) {
        fprintf(stderr, "sandbox.mmap failed\n");
        munmap(host_buf, BUF_SIZE);
        close(memfd);
        return 1;
    }

    printf("Shared buffer mapped in sandbox at %p\n\n", sandbox_buf);

    // Call fill_buffer in the sandbox to fill with 0xAB
    printf("Calling fill_buffer(buf, %d, 0xAB) in sandbox...\n", BUF_SIZE);
    sandbox.call<void(void*, size_t, unsigned char)>(
        "fill_buffer", sandbox_buf, static_cast<size_t>(BUF_SIZE),
        static_cast<unsigned char>(0xAB));

    printf(
        "First 8 bytes after fill: %02x %02x %02x %02x %02x %02x %02x %02x\n\n",
        host_buf[0], host_buf[1], host_buf[2], host_buf[3], host_buf[4],
        host_buf[5], host_buf[6], host_buf[7]);

    // Call increment_buffer in the sandbox
    printf("Calling increment_buffer(buf, %d) in sandbox...\n", BUF_SIZE);
    sandbox.call<void(unsigned char*, size_t)>(
        "increment_buffer", static_cast<unsigned char*>(sandbox_buf),
        static_cast<size_t>(BUF_SIZE));

    printf(
        "First 8 bytes after increment: %02x %02x %02x %02x %02x %02x %02x "
        "%02x\n\n",
        host_buf[0], host_buf[1], host_buf[2], host_buf[3], host_buf[4],
        host_buf[5], host_buf[6], host_buf[7]);

    // Modify from host
    printf("Modifying first byte from host to 0xFF...\n");
    host_buf[0] = 0xFF;

    // Call increment again
    printf("Calling increment_buffer again...\n");
    sandbox.call<void(unsigned char*, size_t)>(
        "increment_buffer", static_cast<unsigned char*>(sandbox_buf),
        static_cast<size_t>(BUF_SIZE));

    printf(
        "First 8 bytes after host modify + increment: %02x %02x %02x %02x %02x "
        "%02x %02x %02x\n",
        host_buf[0], host_buf[1], host_buf[2], host_buf[3], host_buf[4],
        host_buf[5], host_buf[6], host_buf[7]);
    printf("(First byte wrapped from 0xFF to 0x00)\n\n");

    // Cleanup
    munmap(host_buf, BUF_SIZE);
    close(memfd);

    printf("Done!\n");
    return 0;
}
