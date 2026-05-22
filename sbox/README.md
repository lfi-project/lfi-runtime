# SBox

SBox is a C++ library for easily making calls to sandboxed libraries. It
supports passthrough (no sandboxing) and LFI sandboxes.

The goal is that when you migrate a library in your application to be called
through SBox, the API for running the library with any of these backends is the
same, so it is easy to swap between them depending on your needs.

SBox supports multithreading, callbacks, in/out/inout helpers for pointer
parameters, and shared memory between the host and sandbox.

## Example

Given a C library (`lib_add.c`):

```c
int add(int a, int b) {
    return a + b;
}
```

For the process-based sandbox, compile this file to an `add_sandbox` executable
by linking it with `libpbox_sandbox.a`.

You can then sandbox and call it from C++:

```cpp
#include <cstdio>
#include "sbox/lfi.h"
#include "lib_add.h"  // declares: int add(int a, int b);

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libadd.lfi");

    // Call by name (signature deduced from header)
    int result = sandbox.call(SBOX_FN(add), 2, 3);
    printf("add(2, 3) = %d\n", result);

    // Use a function handle for repeated calls
    auto add_fn = sandbox.fn(SBOX_FN(add));
    printf("add(100, 200) = %d\n", add_fn(100, 200));
}
```

You can also specify the signature explicitly instead of using `SBOX_FN`:

```cpp
int result = sandbox.call<int(int, int)>("add", 2, 3);
auto add_fn = sandbox.fn<int(int, int)>("add");
```

For multiple LFI sandboxes, pre-initialize the engine with the desired capacity:

```cpp
sbox::LFIManager::init(4);  // room for 4 sandboxes
auto sb1 = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");
auto sb2 = sbox::Sandbox<sbox::LFI>::create("./libbar.lfi");
```

### Compiling your library

Use the LFI cross-compiler to produce a `.lfi` binary:

```sh
x86_64_lfi-linux-musl-clang -static-pie -o libfoo.lfi foo.c \
    -lboxrt -Wl,--export-dynamic -O2
```

The `-lboxrt` flag links the sandbox runtime, and `-Wl,--export-dynamic`
ensures symbols are visible for lookup.

## Features

### Sandbox Pointer Types

SBox uses two pointer wrapper types for taint tracking of sandbox pointers.

* `sbox<T*>`: an unchecked sandbox pointer. Returned by sandbox function calls,
  and can be passed to sandbox functions that take pointers. Cannot be
  dereferenced by the host.
* `sbox_safe<T*>` -- a bounds-checked sandbox pointer. Returned by `alloc()`
  (on the LFI backend) and by calling `verify()` on a `sbox<T*>`. Freely
  dereferenceable, and can also be implicitly promoted back to `sbox<T*>`.

Raw pointers are rejected at compile time when passed to sandbox calls.

```cpp
auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");

sbox_safe<int*> buf = sandbox->alloc<int>(10);    // verified, safe to use
buf[0] = 42;

sbox<int*> ptr = sandbox->call(SBOX_FN(get_ptr), buf);  // unchecked
sbox_safe<int*> safe = sandbox->verify(ptr, 10);             // now verified
safe[0] = 42;
```

### In/Out Parameters

When a sandbox function takes pointer parameters, use a `CallContext` with
`in()`, `out()`, and `inout()` helpers to automatically manage copying data
between the host and sandbox.

```cpp
auto ctx = sandbox.context();
int counter = 5;
sandbox.call(ctx, SBOX_FN(increment), ctx.inout(counter));
// counter is now 6

int result;
sandbox.call(ctx, SBOX_FN(add_to_result), ctx.in(a), ctx.in(b), ctx.out(result));
```

### Callbacks

Register host functions as callbacks that sandbox code can invoke:

```cpp
static int my_adder(int a, int b) {
    return a + b;
}

auto cb = sandbox.register_callback(my_adder);
int result = sandbox.call(SBOX_FN(process_data), 42, cb);
```

### Shared Memory

Map shared memory into both the host and sandbox for zero-copy data exchange:

```cpp
int memfd = memfd_create("shared_buffer", 0);
ftruncate(memfd, 4096);

// Map in host
auto *host_buf = (unsigned char*)mmap(nullptr, 4096,
    PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);

// Map in sandbox (same fd, same contents)
void *sandbox_buf = sandbox.mmap(nullptr, 4096,
    PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);

// Sandbox writes are visible to the host and vice versa
sandbox.call(SBOX_FN(fill_buffer), sandbox_buf, (size_t)4096, (unsigned char)0xAB);
// host_buf now contains 0xAB bytes
```
