#pragma once

#include "sbox.h"

#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

namespace sbox {

template<>
class CallContext<Passthrough> {
public:
    explicit CallContext(Sandbox<Passthrough>&) {
    }

    CallContext(const CallContext&) = delete;
    CallContext& operator=(const CallContext&) = delete;

    template<typename T>
    detail::context_ptr_t<T> out(T& host_ref) {
        static_assert(!std::is_const_v<T>,
                      "out requires a non-const variable");
        static_assert(detail::context_value_ok<T>,
                      "out requires a trivially-copyable type that is not "
                      "a raw or sbox_safe pointer");
        return detail::context_ptr_t<T>(&host_ref);
    }

    // Passthrough points at the argument itself, so temporaries are
    // rejected on both backends.
    template<typename T>
    void in(const T&&) = delete;

    template<typename T>
    detail::context_ptr_t<const T> in(const T& host_ref) {
        static_assert(detail::context_value_ok<T>,
                      "in requires a trivially-copyable type that is not "
                      "a raw or sbox_safe pointer");
        return detail::context_ptr_t<const T>(&host_ref);
    }

    template<typename T>
    detail::context_ptr_t<T> inout(T& host_ref) {
        static_assert(!std::is_const_v<T>,
                      "inout requires a non-const variable");
        static_assert(detail::context_value_ok<T>,
                      "inout requires a trivially-copyable type that is not "
                      "a raw or sbox_safe pointer");
        return detail::context_ptr_t<T>(&host_ref);
    }
};

// Passthrough backend: loads library via dlopen or static mode.
template<>
class Sandbox<Passthrough> {
public:
    // Dynamic mode: loads library via dlopen. Aborts if loading fails.
    explicit Sandbox(const char* library_path) {
        handle_ = dlopen(library_path, RTLD_NOW);
        if (!handle_) {
            fprintf(stderr, "sbox: dlopen failed: %s\n", dlerror());
            abort();
        }
    }

    // Static mode: no library loading, use direct function pointers.
    Sandbox() = default;

    ~Sandbox() {
        idmem_reset();
        if (handle_) {
            dlclose(handle_);
        }
    }

    // Non-copyable.
    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;

    // Non-movable.
    Sandbox(Sandbox&&) = delete;
    Sandbox& operator=(Sandbox&&) = delete;

    // Call a function by name (dynamic mode).
    template<typename Sig, typename... Args>
    auto call(const char* name, Args... args) {
        void* fn = lookup(name);
        using Ret = detail::sig_return_t<Sig>;
        if constexpr (std::is_void_v<Ret>) {
            call_ptr_sig<Sig>(fn, args...);
        } else {
            return detail::wrap_sbox_return(call_ptr_sig<Sig>(fn, args...));
        }
    }

    // Call with TypedName (dynamic mode, signature deduced from declaration).
    template<typename Ret, typename... Params, typename... Args>
    auto call(TypedName<Ret (*)(Params...)> tn, Args... args) {
        static_assert(sizeof...(Params) == sizeof...(Args),
                      "Wrong number of arguments for sandboxed function");
        static_assert(
            (detail::check_sbox_ptr_arg_v<Params, Args> && ...),
            "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
            "matching type, and parameters cannot be references");
        return call<Ret(Params...)>(tn.name, args...);
    }

    // Call a function by pointer (static mode).
    template<typename Sig, typename... Args>
    auto call(Sig* fn, Args... args) {
        check_static_args<Sig, Args...>();
        detail::strip_noexcept_t<Sig>* f = fn;
        return call_static(f, args...);
    }

    // Context-aware call by name.
    template<typename Sig, typename... Args>
    auto call(CallContext<Passthrough>& ctx, const char* name, Args... args) {
        (void) ctx;
        return this->template call<Sig>(name, args...);
    }

    // Context-aware call with TypedName.
    template<typename Ret, typename... Params, typename... Args>
    auto call(CallContext<Passthrough>& ctx, TypedName<Ret (*)(Params...)> tn,
              Args... args) {
        static_assert(sizeof...(Params) == sizeof...(Args),
                      "Wrong number of arguments for sandboxed function");
        static_assert(
            (detail::check_sbox_ptr_arg_v<Params, Args> && ...),
            "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
            "matching type, and parameters cannot be references");
        return call<Ret(Params...)>(ctx, tn.name, args...);
    }

    // Context-aware call by pointer (static mode).
    template<typename Sig, typename... Args>
    auto call(CallContext<Passthrough>& ctx, Sig* fn, Args... args) {
        (void) ctx;
        return call(fn, args...);
    }

    // Create a call context for in/out/inout parameters.
    CallContext<Passthrough> context() {
        return CallContext<Passthrough>(*this);
    }

    // Get a function handle for repeated calls (dynamic mode).
    template<typename Sig>
    FnHandle<Passthrough, Sig> fn(const char* name) {
        return FnHandle<Passthrough, Sig>(*this, lookup(name));
    }

    // Get a function handle with TypedName (signature deduced from declaration).
    template<typename Ret, typename... Params>
    FnHandle<Passthrough, Ret(Params...)> fn(TypedName<Ret (*)(Params...)> tn) {
        return FnHandle<Passthrough, Ret(Params...)>(*this, lookup(tn.name));
    }

    // Get a function handle by pointer (static mode).
    template<typename Sig>
    FnHandle<Passthrough, detail::strip_noexcept_t<Sig>> fn(Sig* f) {
        return FnHandle<Passthrough, detail::strip_noexcept_t<Sig>>(
            *this, reinterpret_cast<void*>(f));
    }

private:
    template<typename B, typename S>
    friend class FnHandle;

    // Call via function pointer (used by FnHandle).
    template<typename Ret, typename... Args>
    Ret call_ptr(void* fn, Args... args) {
        detail::ScopedSandboxTLS _tls_guard(this);
        using FnPtr = Ret (*)(Args...);
        return reinterpret_cast<FnPtr>(fn)(args...);
    }

public:

    // Memory allocation within the "sandbox" (just regular malloc for
    // passthrough). Returns sbox_safe<T*> since all memory is in the same
    // address space and directly dereferenceable.
    template<typename T>
    sbox_safe<T*> alloc(size_t count = 1) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        size_t bytes;
        if (!detail::checked_mul(count, sizeof(T), &bytes))
            return {};
        return sbox_safe<T*>(static_cast<T*>(std::malloc(bytes)));
    }

    template<typename T>
    sbox_safe<T*> calloc(size_t count) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        return sbox_safe<T*>(static_cast<T*>(std::calloc(count, sizeof(T))));
    }

    template<typename T>
    sbox_safe<T*> realloc(sbox_safe<T*> ptr, size_t count) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        size_t bytes;
        if (!detail::checked_mul(count, sizeof(T), &bytes))
            return {};
        return sbox_safe<T*>(
            static_cast<T*>(std::realloc(ptr.data(), bytes)));
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

    // Verify an unchecked sandbox pointer (promote to sbox_safe). For
    // passthrough, no bounds to check.
    template<typename T>
    sbox_safe<T*> verify(sbox<T*> ptr, size_t count) {
        T* raw = ptr.unsafe_unverified();
        if (!raw && count != 0) {
            fprintf(stderr, "sbox: verify failed: null pointer\n");
            abort();
        }
        return sbox_safe<T*>(raw);
    }

    // Memory mapping.
    void* mmap(void* addr, size_t length, int prot, int flags, int fd,
               off_t offset) {
        return ::mmap(addr, length, prot, flags, fd, offset);
    }

    int munmap(void* addr, size_t length) {
        return ::munmap(addr, length);
    }

    template<typename T>
    T* idmem_alloc(size_t count = 1) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        static_assert(detail::sandbox_value_ok<T>,
                      "idmem_alloc requires a trivially-copyable type that "
                      "holds no raw or sbox_safe pointers");
        size_t bytes;
        if (!detail::checked_mul(count, sizeof(T), &bytes))
            return nullptr;
        void* p = std::malloc(bytes);
        if (!p)
            return nullptr;
        std::lock_guard<std::mutex> lock(idmem_mutex_);
        idmem_allocations_.push_back(p);
        return static_cast<T*>(p);
    }

    void idmem_reset() {
        std::lock_guard<std::mutex> lock(idmem_mutex_);
        for (void* p : idmem_allocations_)
            std::free(p);
        idmem_allocations_.clear();
    }

    // Passthrough keeps no per-thread state.
    void detach_thread() {}

    void copy_to(void* sandbox_dest, const void* host_src, size_t n) {
        std::memcpy(sandbox_dest, host_src, n);
    }

    template<typename T>
    void copy_to(sbox<T*> sandbox_dest, const void* host_src, size_t n) {
        if (n == 0)
            return;
        std::memcpy(sandbox_dest.unsafe_unverified(), host_src, n);
    }
    template<typename T>
    void copy_to(sbox_safe<T*> d, const void* s, size_t n) {
        copy_to(sbox<T*>(d), s, n);
    }

    template<typename U>
    void copy_from(U* host_dest, const void* sandbox_src, size_t n) {
        static_assert(std::is_void_v<U> || detail::sandbox_value_ok<U>,
                      "copy_from destination must be a trivially-copyable "
                      "type that holds no raw or sbox_safe pointers");
        std::memcpy(host_dest, sandbox_src, n);
    }

    template<typename U, typename T>
    void copy_from(U* host_dest, sbox<T*> sandbox_src, size_t n) {
        static_assert(std::is_void_v<U> || detail::sandbox_value_ok<U>,
                      "copy_from destination must be a trivially-copyable "
                      "type that holds no raw or sbox_safe pointers");
        if (n == 0)
            return;
        std::memcpy(host_dest, sandbox_src.unsafe_unverified(), n);
    }
    template<typename U, typename T>
    void copy_from(U* d, sbox_safe<T*> s, size_t n) {
        copy_from(d, sbox<T*>(s), n);
    }

    sbox_safe<char*> copy_string(const char* s) {
        size_t len = std::strlen(s) + 1;
        auto buf = alloc<char>(len);
        if (!buf)
            return {};
        copy_to(buf, s, len);
        return buf;
    }

    // Copy a null-terminated string into a host std::string, scanning at
    // most `n` characters. Returns an empty string if the pointer is
    // null or no terminator is found within `n` characters. The returned
    // string is always null-terminated.
    std::string read_string(sbox<char*> ptr, size_t n) {
        return read_string(sbox<const char*>(ptr.unsafe_unverified()), n);
    }

    std::string read_string(sbox<const char*> ptr, size_t n) {
        const char* raw = ptr.unsafe_unverified();
        if (!raw)
            return {};
        size_t len = strnlen(raw, n);
        if (len == n)
            return {};
        return std::string(raw, len);
    }

    // Copy `count` elements into a host vector. Passthrough has no
    // bounds-checking; returns an empty vector only if the pointer is
    // null.
    template<typename T>
    std::vector<std::remove_cv_t<T>> read_buffer(sbox<T*> ptr, size_t count) {
        static_assert(!std::is_same_v<std::remove_cv_t<T>, bool>,
                      "read_buffer does not support bool elements; read them "
                      "as unsigned char");
        static_assert(detail::sandbox_value_ok<std::remove_cv_t<T>>,
                      "read_buffer requires a trivially-copyable element that "
                      "holds no raw or sbox_safe pointers");
        T* raw = ptr.unsafe_unverified();
        size_t bytes;
        if (!raw || !detail::checked_mul(count, sizeof(T), &bytes))
            return {};
        std::vector<std::remove_cv_t<T>> out(count);
        std::memcpy(out.data(), raw, bytes);
        return out;
    }

    // Register a callback with thunk (for callbacks with sbox<T*> args).
    template<auto fn>
    auto register_callback() {
        static_assert(
            detail::callback_sandbox_matches_v<Passthrough, decltype(fn)>,
            "Callback Sandbox parameter must match this backend");
        using Thunk =
            detail::callback_thunk_impl<Passthrough, decltype(fn), fn>;
        using CType = typename Thunk::c_type;
        return sbox<CType>(reinterpret_cast<CType>(&Thunk::call));
    }

    // Register a callback (passthrough just returns the function pointer).
    // Pointer parameters are not allowed, so callbacks that receive sandbox
    // pointers must take sbox<T*> and be registered with
    // register_callback<fn>().
    template<typename Ret, typename... Args>
    sbox<Ret (*)(Args...)> register_callback(Ret (*fn)(Args...)) {
        static_assert(!detail::is_sbox_ptr_v<Ret> &&
                          !detail::is_sbox_safe_ptr_v<Ret>,
                      "Callbacks returning sbox pointers must be registered "
                      "with register_callback<fn>()");
        static_assert(detail::callback_enums_ok<Args...>,
                      "Callback enum parameters must be scoped with a "
                      "non-bool underlying type");
        static_assert(detail::no_raw_ptr_params<Args...>,
                      "Callback pointer and reference parameters must be "
                      "sbox<T*> and registered with register_callback<fn>()");
        return sbox<Ret (*)(Args...)>(fn);
    }

    // Passthrough has no sandbox stack, so callbacks need no bookkeeping.
    struct CallbackScope {};

    // Returns dlopen handle.
    void* native_handle() const {
        return handle_;
    }

private:
    // Call a function pointer directly, converting arguments as the dynamic
    // backends do.
    template<typename Ret, typename... Params, typename... Args>
    auto call_static(Ret (*fn)(Params...), Args... args) {
        detail::ScopedSandboxTLS _tls_guard(this);
        if constexpr (std::is_void_v<Ret>) {
            fn(detail::convert_call_arg<Params>(args)...);
        } else {
            return detail::wrap_sbox_return(
                fn(detail::convert_call_arg<Params>(args)...));
        }
    }

    template<typename Sig, typename... Args>
    static void check_static_args() {
        using Check =
            detail::sig_args_check<detail::strip_noexcept_t<Sig>, Args...>;
        static_assert(Check::count_ok,
                      "Wrong number of arguments for sandboxed function");
        static_assert(Check::ptrs_ok(),
                      "Pointer arguments must be sbox<T*> or sbox_safe<T*> "
                      "with a matching type, and parameters cannot be references");
    }

    template<typename Ret, typename... Params, typename... Args>
    Ret call_ptr_sig(void* fn, Ret (*)(Params...), Args... args) {
        detail::ScopedSandboxTLS _tls_guard(this);
        return reinterpret_cast<Ret (*)(Params...)>(fn)(
            detail::convert_call_arg<Params>(args)...);
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
        if (!sym) {
            fprintf(stderr, "sbox: symbol not found: %s\n", name);
            abort();
        }
        symbol_cache_[name] = sym;
        return sym;
    }

    void* handle_ = nullptr;
    std::unordered_map<std::string, void*> symbol_cache_;
    std::mutex cache_mutex_;
    std::vector<void*> idmem_allocations_;
    std::mutex idmem_mutex_;
};

} // namespace sbox
