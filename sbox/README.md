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

Compile this file to `libadd.lfi` as described in
[Compiling your library](#compiling-your-library), then sandbox and call it
from C++:

```cpp
#include <cstdio>
#include "sbox/lfi.h"
#include "lib_add.h"  // declares: int add(int a, int b);

int main() {
    auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libadd.lfi");

    // Call by name (signature deduced from header)
    int result = sandbox->call(SBOX_FN(add), 2, 3);
    printf("add(2, 3) = %d\n", result);

    // Use a function handle for repeated calls
    auto add_fn = sandbox->fn(SBOX_FN(add));
    printf("add(100, 200) = %d\n", add_fn(100, 200));
}
```

You can also specify the signature explicitly instead of using `SBOX_FN`:

```cpp
int result = sandbox->call<int(int, int)>("add", 2, 3);
auto add_fn = sandbox->fn<int(int, int)>("add");
```

* `SBOX_FN` and an explicit signature cannot be combined in one call.
* On the LFI backend, arguments and return values are scalars or pointers of
  at most 8 bytes, and a call takes at most 10 arguments.
* Enum values the sandbox supplies must use scoped enums with a non-`bool`
  underlying type.

For multiple LFI sandboxes, pre-initialize the engine with the desired capacity:

```cpp
sbox::LFIManager::init(4);  // reserve space for 4 sandboxes
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

* The minimal Linux runtime has no `mmap`, so link a brk-based allocator
  such as minialloc or build the runtime with `-Dlinux_full=true`.

## Features

### Sandbox Pointer Types

SBox uses two pointer wrapper types for taint tracking of sandbox pointers.

* `sbox<T*>`: an unchecked sandbox pointer. Returned by sandbox function calls,
  and can be passed to sandbox functions that take pointers. Cannot be
  dereferenced by the host.
* `sbox_safe<T*>` -- a bounds-checked sandbox pointer. Returned by `alloc()`
  (on the LFI backend) and by calling `verify()` on a `sbox<T*>`. Freely
  dereferenceable, and can also be implicitly promoted back to `sbox<T*>`.

Raw pointers are rejected at compile time when passed to calls made with
`SBOX_FN` or through a function handle.

```cpp
auto sandbox = sbox::Sandbox<sbox::LFI>::create("./libfoo.lfi");

sbox::sbox_safe<int*> buf = sandbox->alloc<int>(10);  // verified, safe to use
buf[0] = 42;

sbox::sbox<int*> ptr = sandbox->call(SBOX_FN(get_ptr), buf);  // unchecked
sbox::sbox_safe<int*> safe = sandbox->verify(ptr, 10);       // now verified
safe[0] = 42;
```

* `verify()` aborts unless the buffer is inside the sandbox and aligned, and
  accepts a null pointer only with a count of zero.
* Sandbox memory cannot be typed as raw pointers, `sbox_safe`, `bool`,
  polymorphic types or unscoped enums, so store pointers as `sbox<T*>`.
* Host accesses through a verified pointer can fault if the sandbox unmaps
  or write-protects that memory.
* `read_string()` returns an empty string for a string outside the sandbox.
* After `using namespace sbox`, write the pointer type as `::sbox::sbox<T*>`.

### In/Out Parameters

When a sandbox function takes pointer parameters, use a `CallContext` with
`in()`, `out()`, and `inout()` helpers to automatically manage copying data
between the host and sandbox.

```cpp
auto ctx = sandbox->context();
int counter = 5;
sandbox->call(ctx, SBOX_FN(increment), ctx.inout(counter));
// counter is now 6

int result = 0;
sandbox->call(ctx, SBOX_FN(add_to_result), ctx.in(a), ctx.in(b), ctx.out(result));
```

* The helpers accept trivially copyable values, with pointer values held as
  `sbox<T*>`.
* On the LFI backend, `out()` copies the host value in too, so initialize
  variables passed to it.
* Each value is copied back once, after the first context call that receives
  it, or when the context is destroyed if no context call does.
* Values use sandbox stack space until the context is destroyed, so create a
  new context for each iteration of a long loop.
* A context belongs to one thread and one sandbox, and one created in a
  callback must be destroyed before the callback returns.

### Callbacks

Register host functions as callbacks that sandbox code can invoke:

```cpp
static int my_adder(int a, int b) {
    return a + b;
}

auto cb = sandbox->register_callback(my_adder);
int result = sandbox->call(SBOX_FN(process_data), 42, cb);
```

* A callback that receives sandbox pointers takes `sbox<T*>` parameters,
  optionally after a `Sandbox<Backend>&`, and is registered with
  `register_callback<fn>()`.
* On the LFI backend, callback integer parameters are at least 32 bits wide
  and all arguments must fit in registers.
* A callback that takes the sandbox aborts if a thread the library created
  invokes it.
* Each host thread gets its own sandbox context, which is released when the
  thread exits or calls `detach_thread()`.

### Shared Memory

Map shared memory into both the host and sandbox for zero-copy data exchange:

```cpp
int memfd = memfd_create("shared_buffer", 0);
ftruncate(memfd, 4096);

// Map in host
auto *host_buf = (unsigned char*)mmap(nullptr, 4096,
    PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);

// Map in sandbox (same fd, same contents)
sbox::sbox<void*> sandbox_buf(sandbox->mmap(nullptr, 4096,
    PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0));

// Sandbox writes are visible to the host and vice versa
sandbox->call(SBOX_FN(fill_buffer), sandbox_buf, (size_t)4096, (unsigned char)0xAB);
// host_buf now contains 0xAB bytes
```

* The LFI backend refuses executable mappings.
