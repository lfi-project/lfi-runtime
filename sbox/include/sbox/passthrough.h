#pragma once

#include "sbox.h"

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace sbox {

namespace detail {

// Simple arena for passthrough identity memory
class PassthroughArena {
    static constexpr size_t DEFAULT_SIZE = 64 * 1024;  // 64KB default

    char* base_ = nullptr;
    size_t offset_ = 0;

public:
    PassthroughArena() : base_(static_cast<char*>(std::malloc(DEFAULT_SIZE))) {}

    ~PassthroughArena() {
        std::free(base_);
    }

    PassthroughArena(const PassthroughArena&) = delete;
    PassthroughArena& operator=(const PassthroughArena&) = delete;

    void* alloc(size_t size, size_t align = 8) {
        offset_ = (offset_ + align - 1) & ~(align - 1);

        if (!base_ || offset_ + size > DEFAULT_SIZE) {
            return nullptr;
        }

        void* ptr = base_ + offset_;
        offset_ += size;
        return ptr;
    }

    void reset() {
        offset_ = 0;
    }
};

// Thread-local arena storage
inline PassthroughArena& get_thread_arena() {
    thread_local PassthroughArena arena;
    return arena;
}

}  // namespace detail

// Passthrough CallContext - just returns pointers to host variables (zero
// overhead) Defined before Sandbox since it doesn't need Sandbox to be complete
template<>
class CallContext<Passthrough> {
    Sandbox<Passthrough>* sandbox_;

public:
    explicit CallContext(Sandbox<Passthrough>& sb) : sandbox_(&sb) {
    }

    // No-op for passthrough
    void finalize() {
    }

    // Just return pointer to host variable
    template<typename T>
    T* out(T& host_ref) {
        return &host_ref;
    }

    template<typename T>
    const T* in(const T& host_ref) {
        return &host_ref;
    }

    template<typename T>
    T* inout(T& host_ref) {
        return &host_ref;
    }
};

// Passthrough backend - loads library normally via dlopen, or static mode
template<>
class Sandbox<Passthrough> {
public:
    // Dynamic mode - loads library via dlopen
    explicit Sandbox(const char* library_path) {
        handle_ = dlopen(library_path, RTLD_NOW);
    }

    // Static mode - no library loading, use direct function pointers
    Sandbox() = default;

    ~Sandbox() {
        if (handle_) {
            dlclose(handle_);
        }
    }

    // Non-copyable
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    // Non-movable
    Sandbox(Sandbox&&) = delete;
    Sandbox& operator=(Sandbox&&) = delete;

    // Call a function by name (dynamic mode).
    // 'name' must be a string literal (pointer is cached directly).
    template<typename Sig, typename... Args>
    auto call(const char* name, Args... args) {
        void* fn = lookup(name);
        if (!fn) {
            fprintf(stderr, "sbox: symbol not found: %s\n", name);
            abort();
        }
        using Ret = detail::sig_return_t<Sig>;
        if constexpr (std::is_void_v<Ret>) {
            call_ptr_sig<Sig>(fn, args...);
        } else {
            return detail::wrap_sbox_return(call_ptr_sig<Sig>(fn, args...));
        }
    }

    // Call with TypedName (dynamic mode, signature deduced from declaration)
    template<typename Ret, typename... Params, typename... Args>
    auto call(TypedName<Ret (*)(Params...)> tn, Args... args) {
        static_assert(sizeof...(Params) == sizeof...(Args),
                      "Wrong number of arguments for sandboxed function");
        static_assert(
            (detail::check_sbox_ptr_arg_v<Params, Args> && ...),
            "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
            "matching type");
        return call<Ret(Params...)>(tn.name, args...);
    }

    // Call a function by pointer (static mode)
    template<typename Sig, typename... Args>
    auto call(Sig* fn, Args... args) {
        detail::tls_current_sandbox = this;
        if constexpr (std::is_void_v<
                          decltype(fn(detail::unwrap_sbox_arg(args)...))>) {
            fn(detail::unwrap_sbox_arg(args)...);
        } else {
            return detail::wrap_sbox_return(
                fn(detail::unwrap_sbox_arg(args)...));
        }
    }

    // Context-aware call by name
    template<typename Sig, typename... Args>
    auto call(CallContext<Passthrough>& ctx, const char* name, Args... args) {
        using Ret = detail::sig_return_t<Sig>;
        if constexpr (std::is_void_v<Ret>) {
            this->template call<Sig>(name, args...);
            ctx.finalize();
        } else {
            auto result = this->template call<Sig>(name, args...);
            ctx.finalize();
            return result;
        }
    }

    // Context-aware call with TypedName
    template<typename Ret, typename... Params, typename... Args>
    auto call(CallContext<Passthrough>& ctx, TypedName<Ret (*)(Params...)> tn,
              Args... args) {
        static_assert(sizeof...(Params) == sizeof...(Args),
                      "Wrong number of arguments for sandboxed function");
        static_assert(
            (detail::check_sbox_ptr_arg_v<Params, Args> && ...),
            "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
            "matching type");
        return call<Ret(Params...)>(ctx, tn.name, args...);
    }

    // Context-aware call by pointer (static mode)
    template<typename Sig, typename... Args>
    auto call(CallContext<Passthrough>& ctx, Sig* fn, Args... args) {
        if constexpr (std::is_void_v<
                          decltype(fn(detail::unwrap_sbox_arg(args)...))>) {
            fn(detail::unwrap_sbox_arg(args)...);
            ctx.finalize();
        } else {
            auto result = detail::wrap_sbox_return(
                fn(detail::unwrap_sbox_arg(args)...));
            ctx.finalize();
            return result;
        }
    }

    // Create a call context for in/out/inout parameters
    CallContext<Passthrough> context() {
        return CallContext<Passthrough>(*this);
    }

    // Get a function handle for repeated calls (dynamic mode).
    // 'name' must be a string literal (pointer is cached directly).
    template<typename Sig>
    FnHandle<Passthrough, Sig> fn(const char* name) {
        return FnHandle<Passthrough, Sig>(*this, lookup(name));
    }

    // Get a function handle with TypedName (signature deduced from declaration)
    template<typename Ret, typename... Params>
    FnHandle<Passthrough, Ret(Params...)> fn(TypedName<Ret (*)(Params...)> tn) {
        return FnHandle<Passthrough, Ret(Params...)>(*this, lookup(tn.name));
    }

    // Get a function handle by pointer (static mode) - just returns the pointer
    template<typename Sig>
    Sig* fn(Sig* f) {
        return f;
    }

    // Call via function pointer (used by FnHandle)
    template<typename Ret, typename... Args>
    Ret call_ptr(void* fn, Args... args) {
        detail::tls_current_sandbox = this;
        using FnPtr = Ret (*)(Args...);
        return reinterpret_cast<FnPtr>(fn)(args...);
    }

    // Memory allocation within the "sandbox" (just regular malloc for
    // passthrough). Returns sbox_safe<T*> since all memory is in the same
    // address space and directly dereferenceable.
    template<typename T>
    sbox_safe<T*> alloc(size_t count = 1) {
        return sbox_safe<T*>(static_cast<T*>(std::malloc(sizeof(T) * count)));
    }

    template<typename T>
    sbox_safe<T*> calloc(size_t count) {
        return sbox_safe<T*>(static_cast<T*>(std::calloc(count, sizeof(T))));
    }

    template<typename T>
    sbox_safe<T*> realloc(sbox_safe<T*> ptr, size_t count) {
        return sbox_safe<T*>(
            static_cast<T*>(std::realloc(ptr.data(), sizeof(T) * count)));
    }

    void free(void* ptr) {
        std::free(ptr);
    }

    template<typename T>
    void free(sbox<T*> p) {
        std::free(p.unsafe_unverified());
    }
    template<typename T>
    void free(sbox_safe<T*> p) {
        free(sbox<T*>(p));
    }

    // Verify an unchecked sandbox pointer (promote to sbox_safe).
    // For passthrough, no bounds to check.
    template<typename T>
    sbox_safe<T*> verify(sbox<T*> ptr, size_t count) {
        (void) count;
        return sbox_safe<T*>(ptr.unsafe_unverified());
    }

    // Memory mapping
    void* mmap(void* addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
        return ::mmap(addr, length, prot, flags, fd, offset);
    }

    int munmap(void* addr, size_t length) {
        return ::munmap(addr, length);
    }

    // Identity-mapped memory (trivial for passthrough - all memory is shared)
    void* mmap_identity(size_t length, int prot) {
        return ::mmap(nullptr, length, prot, MAP_PRIVATE | MAP_ANONYMOUS, -1,
                      0);
    }

    int munmap_identity(void* addr, size_t length) {
        return ::munmap(addr, length);
    }

    // Arena allocator for identity-mapped memory (per-thread)
    template<typename T>
    T* idmem_alloc(size_t count = 1) {
        return static_cast<T*>(
            detail::get_thread_arena().alloc(sizeof(T) * count, alignof(T)));
    }

    void idmem_reset() {
        detail::get_thread_arena().reset();
    }

    // File descriptor registration (no-op for passthrough)
    int register_fd(int fd) {
        return fd;
    }

    // Close a file descriptor
    int close_fd(int fd) {
        return ::close(fd);
    }

    // Data transfer (trivial memcpy for passthrough)
    void copy_to(void* sandbox_dest, const void* host_src, size_t n) {
        std::memcpy(sandbox_dest, host_src, n);
    }

    template<typename T>
    void copy_to(sbox<T*> sandbox_dest, const void* host_src, size_t n) {
        std::memcpy(sandbox_dest.unsafe_unverified(), host_src, n);
    }
    template<typename T>
    void copy_to(sbox_safe<T*> d, const void* s, size_t n) {
        copy_to(sbox<T*>(d), s, n);
    }

    void copy_from(void* host_dest, const void* sandbox_src, size_t n) {
        std::memcpy(host_dest, sandbox_src, n);
    }

    template<typename T>
    void copy_from(void* host_dest, sbox<T*> sandbox_src, size_t n) {
        std::memcpy(host_dest, sandbox_src.unsafe_unverified(), n);
    }
    template<typename T>
    void copy_from(void* d, sbox_safe<T*> s, size_t n) {
        copy_from(d, sbox<T*>(s), n);
    }

    // String helper
    sbox_safe<char*> copy_string(const char* s) {
        size_t len = std::strlen(s) + 1;
        auto buf = alloc<char>(len);
        if (!buf)
            return {};
        copy_to(buf, s, len);
        return buf;
    }

    // Register a callback with thunk (for callbacks with sbox<T*> args)
    template<auto fn>
    auto register_callback() {
        using Thunk = detail::callback_thunk_impl<decltype(fn), fn>;
        using CType = typename Thunk::c_type;
        return sbox<CType>(reinterpret_cast<CType>(&Thunk::call));
    }

    // Register a callback (passthrough just returns the function pointer)
    template<typename Ret, typename... Args>
    sbox<Ret (*)(Args...)> register_callback(Ret (*fn)(Args...)) {
        return sbox<Ret (*)(Args...)>(fn);
    }

    // Escape hatch for advanced usage (returns dlopen handle)
    void* native_handle() const {
        return handle_;
    }

private:
    // Convert argument, unwrapping sbox types and using reinterpret_cast for
    // pointer conversions
    template<typename To, typename From>
    static To convert_arg(From arg) {
        if constexpr (detail::is_sbox_ptr_v<From>) {
            return reinterpret_cast<To>(arg.unsafe_unverified());
        } else if constexpr (detail::is_sbox_safe_ptr_v<From>) {
            return reinterpret_cast<To>(arg.data());
        } else if constexpr (std::is_pointer_v<To> && std::is_pointer_v<From>) {
            return reinterpret_cast<To>(arg);
        } else {
            return static_cast<To>(arg);
        }
    }

    template<typename Ret, typename... Params, typename... Args>
    Ret call_ptr_sig(void* fn, Ret (*)(Params...), Args... args) {
        detail::tls_current_sandbox = this;
        return reinterpret_cast<Ret (*)(Params...)>(fn)(
            convert_arg<Params>(args)...);
    }

    template<typename Sig, typename... Args>
    auto call_ptr_sig(void* fn, Args... args) {
        return call_ptr_sig(fn, static_cast<Sig*>(nullptr), args...);
    }

    void* lookup(const char* name) {
        std::lock_guard<std::mutex> lock(cache_mutex_);

        auto it = symbol_cache_.find(name);
        if (it != symbol_cache_.end()) {
            return it->second;
        }

        void* sym = dlsym(handle_, name);
        if (sym) {
            symbol_cache_[name] = sym;
        }
        return sym;
    }

    void* handle_ = nullptr;
    std::unordered_map<const char*, void*> symbol_cache_;
    std::mutex cache_mutex_;
};

}  // namespace sbox
