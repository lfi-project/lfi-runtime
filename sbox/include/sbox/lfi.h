#pragma once

#include "sbox.h"

#ifdef SBOX_STATIC
#error "SBOX_STATIC cannot be used with the LFI backend"
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

extern "C" {
#include "lfi_arch.h"
#include "lfi_core.h"
#include "lfi_linux.h"
}

namespace sbox {

namespace detail {

// Maximum number of arguments supported for LFI calls.
constexpr size_t max_args = 10;

// Classify whether an argument is float/double.
template<typename T>
constexpr bool is_float_arg =
    std::is_floating_point_v<std::remove_cv_t<std::remove_reference_t<T>>>;

#if defined(__x86_64__)
constexpr size_t max_int_reg_args = 6;
constexpr size_t max_float_reg_args = 8;

inline void set_int_arg(LFIRegs* regs, size_t index, uint64_t value) {
    switch (index) {
        case 0: regs->rdi = value; break;
        case 1: regs->rsi = value; break;
        case 2: regs->rdx = value; break;
        case 3: regs->rcx = value; break;
        case 4: regs->r8 = value; break;
        case 5: regs->r9 = value; break;
    }
}

inline void set_float_arg(LFIRegs* regs, size_t index, uint64_t value) {
    regs->xmm[index * 2] = value;
}

inline uint64_t get_int_return(LFIRegs* regs) { return regs->rax; }
inline uint64_t get_float_return(LFIRegs* regs) { return regs->xmm[0]; }
inline uint64_t get_sp(LFIRegs* regs) { return regs->rsp; }
inline void set_sp(LFIRegs* regs, uint64_t sp) { regs->rsp = sp; }

#elif defined(__aarch64__)
constexpr size_t max_int_reg_args = 8;
constexpr size_t max_float_reg_args = 8;

inline void set_int_arg(LFIRegs* regs, size_t index, uint64_t value) {
    switch (index) {
        case 0: regs->x0 = value; break;
        case 1: regs->x1 = value; break;
        case 2: regs->x2 = value; break;
        case 3: regs->x3 = value; break;
        case 4: regs->x4 = value; break;
        case 5: regs->x5 = value; break;
        case 6: regs->x6 = value; break;
        case 7: regs->x7 = value; break;
    }
}

inline void set_float_arg(LFIRegs* regs, size_t index, uint64_t value) {
    regs->vector[index * 2] = value;
}

inline uint64_t get_int_return(LFIRegs* regs) { return regs->x0; }
inline uint64_t get_float_return(LFIRegs* regs) { return regs->vector[0]; }
inline uint64_t get_sp(LFIRegs* regs) { return regs->sp; }
inline void set_sp(LFIRegs* regs, uint64_t sp) { regs->sp = sp; }

#else
#error "Unsupported architecture for LFI backend"
#endif

// Arguments and return values must be scalars that fit in one register.
template<typename T>
constexpr bool is_reg_type_impl() {
    if constexpr (std::is_arithmetic_v<T> || std::is_enum_v<T> ||
                  std::is_pointer_v<T>) {
        return sizeof(T) <= 8;
    } else {
        return false;
    }
}

template<typename T>
inline constexpr bool is_reg_type = is_reg_type_impl<T>();

// Convert an integral, enum, or pointer value to a register value. Narrow
// integers are extended according to their signedness.
template<typename T>
uint64_t to_reg(T arg) {
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<uint64_t>(arg);
    } else if constexpr (std::is_enum_v<T>) {
        return to_reg(static_cast<std::underlying_type_t<T>>(arg));
    } else if constexpr (std::is_signed_v<T>) {
        return static_cast<uint64_t>(static_cast<int64_t>(arg));
    } else {
        return static_cast<uint64_t>(arg);
    }
}

// Collects overflow args that don't fit in registers.
struct MarshalResult {
    uint64_t stack_args[max_args];
    size_t n_stack = 0;
};

// Marshal a single argument into a register or the stack overflow buffer.
template<typename T>
void marshal_arg(LFIRegs* regs, MarshalResult& result,
                 size_t& int_idx, size_t& float_idx, T arg) {
    static_assert(is_reg_type<T>,
                  "LFI call arguments must be scalars of at most 8 bytes");
    uint64_t val = 0;
    if constexpr (is_float_arg<T>) {
        std::memcpy(&val, &arg, sizeof(arg));
        if (float_idx < max_float_reg_args) {
            set_float_arg(regs, float_idx++, val);
        } else {
            result.stack_args[result.n_stack++] = val;
        }
    } else {
        val = to_reg(arg);
        if (int_idx < max_int_reg_args) {
            set_int_arg(regs, int_idx++, val);
        } else {
            result.stack_args[result.n_stack++] = val;
        }
    }
}

// Marshal all arguments. Returns info about any overflow stack args.
template<typename... Args>
MarshalResult marshal_args_lfi([[maybe_unused]] LFIRegs* regs, Args... args) {
    static_assert(sizeof...(Args) <= max_args,
                  "Too many arguments (max is sbox::detail::max_args)");
    MarshalResult result{};
    [[maybe_unused]] size_t int_idx = 0;
    [[maybe_unused]] size_t float_idx = 0;
    (marshal_arg(regs, result, int_idx, float_idx, args), ...);
    return result;
}

// Write overflow stack args directly onto the sandbox stack and adjust SP.
// Works because LFI sandbox memory is in the host address space. The
// trampoline loads the adjusted SP and jumps to the target; the callee then
// sees stack args at the platform's standard ABI offsets (RSP+8.. on x86-64,
// SP+0.. on aarch64, accounting for the trampoline's own SP usage). Returns
// false without writing if the staging area is not inside the sandbox.
inline bool stage_stack_args(LFIBox* box, LFIRegs* regs,
                             const uint64_t* stack_args, size_t n_stack) {
    if (n_stack == 0)
        return true;

    uint64_t rsp = get_sp(regs);
    uint64_t aligned = (rsp - 8 * n_stack) & ~0xFULL;
    if (!lfi_box_bufvalid(box, aligned, 8 * n_stack))
        return false;

    for (size_t i = 0; i < n_stack; i++) {
        *reinterpret_cast<uint64_t*>(aligned + 8 * i) = stack_args[i];
    }

    set_sp(regs, aligned);
    return true;
}

// Per-thread fast-path for resolving the active LFI sandbox context.
// last_sandbox_id 0 means "no cache." Comparison is against the Sandbox's
// monotonically-issued id; a freed Sandbox's id is never reissued, so a
// cache hit guarantees last_ctxp still points into a live Sandbox's map.
inline thread_local uint64_t last_sandbox_id = 0;
inline thread_local LFIContext** last_ctxp = nullptr;

// Monotonic counter for assigning Sandbox<LFI> ids.
inline std::atomic<uint64_t> sandbox_id_counter{1};

// Unique, never-reused id for each host thread, assigned on first use.
// Unlike std::thread::id, it is not recycled when a thread exits, so a stale
// per-thread entry is never found by a later thread.
inline std::atomic<uint64_t> thread_token_counter{1};
inline thread_local uint64_t tls_thread_token = 0;

inline uint64_t thread_token() {
    if (tls_thread_token == 0)
        tls_thread_token =
            thread_token_counter.fetch_add(1, std::memory_order_relaxed);
    return tls_thread_token;
}

struct ThreadAttachment {
    Sandbox<LFI>* sandbox;
    uint64_t sandbox_id;
    uint64_t thread;
};

struct ThreadCleanup {
    std::vector<ThreadAttachment> attachments;
    ~ThreadCleanup();
};

inline thread_local ThreadCleanup tls_cleanup;

// Head of this thread's list of live CallContext<LFI> objects.
inline thread_local CallContext<LFI>* tls_live_contexts = nullptr;

// Set once tls_cleanup has been destroyed on this thread, so that later
// thread-local destructors that use a sandbox do not touch it. Entries those
// calls create are removed only when the Sandbox is destroyed.
inline thread_local bool tls_cleanup_done = false;

// Live Sandbox<LFI> objects by id, each with the number of exiting threads
// currently detaching from it. ThreadCleanup pins a sandbox before detaching
// from it, and the Sandbox destructor waits until no pins remain before
// removing its id.
struct LiveSandboxes {
    std::mutex mutex;
    std::condition_variable unpinned;
    std::unordered_map<uint64_t, size_t> pins;
};

LiveSandboxes& live_sandboxes();

} // namespace detail

// Process-global LFI engine manager. Optionally call init() before creating
// sandboxes to set the capacity. If not called, the first Sandbox<LFI>::create
// creates a default engine with capacity 1.
class LFIManager {
    static inline std::mutex mutex_;
    static inline struct LFIEngine* engine_ = nullptr;
    static inline struct LFILinuxEngine* linux_engine_ = nullptr;

public:
    static bool init(size_t n);

    // Free the engine. Returns false without freeing it if any Sandbox<LFI>
    // exists, including one that is being created or destroyed. Every thread
    // that used a destroyed sandbox must also have exited or detached,
    // because the sandbox's memory is released only then.
    static bool destroy();

private:
    friend class Sandbox<LFI>;

    static inline size_t sandboxes_ = 0;

    static bool create(size_t n);

    // Return the engine, creating it if needed, and count a new sandbox
    // that uses it. Returns nullptr on failure.
    static struct LFILinuxEngine* acquire();
    static void release();
};

template<>
class Sandbox<LFI> {
    LFILinuxProc* proc_ = nullptr;
    LFILinuxThread* main_thread_ = nullptr;
    LFIBox* box_ = nullptr;

    std::mutex symbol_cache_mutex_;
    std::unordered_map<std::string, lfiptr> symbol_cache_;

    uint64_t main_thread_token_ = 0;

    // Per-thread sandbox contexts. The main thread's context is owned by
    // main_thread_. Worker entries hold contexts created by lfi_clone, which
    // the LFI runtime frees when the thread exits or detaches.
    std::mutex thread_ctx_mutex_;
    std::unordered_map<uint64_t, LFIContext*> thread_ctxs_;

    // Unique, never-reused id used by the TLS fast path in get_thread_ctx.
    uint64_t id_ = 0;

    // Whether this sandbox is counted by LFIManager.
    bool holds_engine_ = false;

    explicit Sandbox() = default;

public:
    // Create a sandbox from an LFI binary. Returns nullptr on failure.
    static std::unique_ptr<Sandbox<LFI>> create(const char* library_path);

    ~Sandbox();

    Sandbox(const Sandbox&) = delete;
    Sandbox& operator=(const Sandbox&) = delete;
    Sandbox(Sandbox&&) = delete;
    Sandbox& operator=(Sandbox&&) = delete;

    // Call a function by name.
    template<typename Sig, typename... Args>
    auto call(const char* name, Args... args) {
        void* fn = reinterpret_cast<void*>(lookup(name));
        using Ret = detail::sig_return_t<Sig>;
        if constexpr (std::is_void_v<Ret>) {
            call_ptr_sig<Sig>(fn, args...);
        } else {
            return detail::wrap_sbox_return(call_ptr_sig<Sig>(fn, args...));
        }
    }

    // Call with TypedName (signature deduced from declaration).
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

    // Get a function handle for repeated calls.
    template<typename Sig>
    FnHandle<LFI, Sig> fn(const char* name) {
        return FnHandle<LFI, Sig>(*this, reinterpret_cast<void*>(lookup(name)));
    }

    // Get a function handle with TypedName.
    template<typename Ret, typename... Params>
    FnHandle<LFI, Ret(Params...)> fn(TypedName<Ret (*)(Params...)> tn) {
        return FnHandle<LFI, Ret(Params...)>(
            *this, reinterpret_cast<void*>(lookup(tn.name)));
    }

private:
    template<typename B, typename S>
    friend class FnHandle;

    // Call via function pointer (used by FnHandle).
    template<typename Ret, typename... Args>
    Ret call_ptr(void* fn_ptr, Args... args) {
        static_assert(std::is_void_v<Ret> || detail::is_reg_type<Ret>,
                      "LFI return values must be scalars of at most 8 bytes");
        static_assert((!std::is_reference_v<Args> && ...),
                      "LFI call arguments must be scalars of at most 8 bytes");
        detail::ScopedSandboxTLS _tls_guard(this);

        auto ctxp = get_thread_ctx();

        lfi_set_invoke_info(ctxp, reinterpret_cast<lfiptr>(fn_ptr), box_);

        LFIRegs* regs = lfi_ctx_regs(*ctxp);
        auto result = detail::marshal_args_lfi(regs, args...);

        // Save sandbox RSP before staging. The trampoline saves/restores
        // its own copy, but it saves the already-staged value. We restore
        // to the pre-staging value so that CallContext stack allocations
        // remain valid.
        uint64_t saved_sp = detail::get_sp(regs);
        if (!detail::stage_stack_args(box_, regs, result.stack_args,
                                      result.n_stack)) {
            fprintf(stderr, "sbox: sandbox stack pointer is invalid\n");
            abort();
        }

        lfi_trampoline_struct();

        detail::set_sp(regs, saved_sp);

        if constexpr (!std::is_void_v<Ret>) {
            using R = std::remove_cv_t<Ret>;
            R ret;
            if constexpr (std::is_same_v<R, bool>) {
                ret = (detail::get_int_return(regs) & 0xff) != 0;
            } else if constexpr (std::is_floating_point_v<Ret>) {
                uint64_t raw = detail::get_float_return(regs);
                std::memcpy(&ret, &raw, sizeof(ret));
            } else if constexpr (std::is_pointer_v<R>) {
                ret = reinterpret_cast<R>(detail::get_int_return(regs));
            } else {
                uint64_t raw = detail::get_int_return(regs);
                std::memcpy(&ret, &raw, sizeof(ret));
            }
            return ret;
        }
    }

public:
    // Memory allocation.

    template<typename T>
    sbox_safe<T*> alloc(size_t count = 1) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        size_t bytes;
        if (!detail::checked_mul(count, sizeof(T), &bytes))
            return {};
        detail::ScopedSandboxTLS _tls_guard(this);
        return sbox_safe<T*>(static_cast<T*>(
            lfi_lib_malloc(box_, get_thread_ctx(), bytes)));
    }

    template<typename T>
    sbox_safe<T*> calloc(size_t count) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        detail::ScopedSandboxTLS _tls_guard(this);
        return sbox_safe<T*>(static_cast<T*>(
            lfi_lib_calloc(box_, get_thread_ctx(), count, sizeof(T))));
    }

    template<typename T>
    sbox_safe<T*> realloc(sbox_safe<T*> ptr, size_t count) {
        static_assert(alignof(T) <= 16,
                      "Sandbox allocations are aligned to at most 16 bytes");
        size_t bytes;
        if (!detail::checked_mul(count, sizeof(T), &bytes))
            return {};
        detail::ScopedSandboxTLS _tls_guard(this);
        return sbox_safe<T*>(static_cast<T*>(
            lfi_lib_realloc(box_, get_thread_ctx(), ptr.data(), bytes)));
    }

    void free(void* ptr) {
        detail::ScopedSandboxTLS _tls_guard(this);
        lfi_lib_free(box_, get_thread_ctx(), ptr);
    }

    template<typename T>
    void free(sbox<T*> p) {
        detail::ScopedSandboxTLS _tls_guard(this);
        lfi_lib_free(box_, get_thread_ctx(), p.unsafe_unverified());
    }

    template<typename T>
    void free(sbox_safe<T*> p) {
        free(sbox<T*>(p));
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
        void* p;
        {
            detail::ScopedSandboxTLS _tls_guard(this);
            p = lfi_lib_malloc(box_, get_thread_ctx(), bytes);
        }
        if (!p)
            return nullptr;
        std::lock_guard<std::mutex> lock(idmem_mutex_);
        idmem_allocations_.push_back(p);
        return static_cast<T*>(p);
    }

    void idmem_reset();

    // Pointer verification.

    // A null pointer verifies only with a count of zero, and the result is
    // then null. A non-null pointer must be aligned for T.
    template<typename T>
    sbox_safe<T*> verify(sbox<T*> ptr, size_t count) {
        T* raw = ptr.unsafe_unverified();
        size_t bytes;
        bool ok = detail::checked_mul(count, sizeof(T), &bytes) &&
                  (raw ? reinterpret_cast<uintptr_t>(raw) % alignof(T) == 0 &&
                             lfi_box_bufvalid(
                                 box_, reinterpret_cast<lfiptr>(raw), bytes)
                       : bytes == 0);
        if (!ok) {
            fprintf(stderr,
                    "sbox: verify failed: pointer %p not in sandbox or "
                    "misaligned\n",
                    static_cast<void*>(raw));
            abort();
        }
        return sbox_safe<T*>(raw);
    }

    // Data transfer.

    // Raw copy where the caller has ensured that the destination is a valid
    // address.
    void copy_to(void* sandbox_dest, const void* host_src, size_t n) {
        std::memcpy(sandbox_dest, host_src, n);
    }

    template<typename T>
    // The destination must also be mapped writable, since the sandbox
    // chose it and a plain copy to read-only memory would fault in the host.
    void copy_to(sbox<T*> sandbox_dest, const void* host_src, size_t n) {
        if (n == 0)
            return;
        void* dest = sandbox_dest.unsafe_unverified();
        check_range(dest, n, "copy_to");
        if (!lfi_box_copyto_checked(box_, reinterpret_cast<lfiptr>(dest),
                                    host_src, n)) {
            fprintf(stderr,
                    "sbox: copy_to failed: [%p, +%zu) not writable\n", dest,
                    n);
            abort();
        }
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

    // A typed destination must be able to hold sandbox data, so sandbox
    // bytes cannot become raw or verified pointers. A void* destination is
    // an untyped byte copy.
    template<typename U, typename T>
    void copy_from(U* host_dest, sbox<T*> sandbox_src, size_t n) {
        static_assert(std::is_void_v<U> || detail::sandbox_value_ok<U>,
                      "copy_from destination must be a trivially-copyable "
                      "type that holds no raw or sbox_safe pointers");
        if (n == 0)
            return;
        const void* src = sandbox_src.unsafe_unverified();
        check_range(src, n, "copy_from");
        std::memcpy(host_dest, src, n);
    }

    template<typename U, typename T>
    void copy_from(U* d, sbox_safe<T*> s, size_t n) {
        copy_from(d, sbox<T*>(s), n);
    }

    sbox_safe<char*> copy_string(const char* s);

    // Copy a null-terminated string out of the sandbox into a host
    // std::string, scanning at most `n` characters. Returns an empty
    // string if the pointer is null, the string is not terminated within
    // `n` characters, or the string does not lie within the sandbox. The
    // returned string is always null-terminated regardless of the sandbox's
    // contents.
    std::string read_string(sbox<char*> ptr, size_t n) {
        return read_string(sbox<const char*>(ptr.unsafe_unverified()), n);
    }

    std::string read_string(sbox<const char*> ptr, size_t n) {
        const char* raw = ptr.unsafe_unverified();
        if (!raw)
            return {};
        size_t len = lfi_box_strnlen(
            box_, reinterpret_cast<lfiptr>(raw), n);
        if (len == SIZE_MAX)
            return {};
        return std::string(raw, len);
    }

    // Copy `count` elements from a sandbox buffer into a host vector.
    // Returns an empty vector if the buffer is not entirely within the
    // sandbox.
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
        if (!raw || !detail::checked_mul(count, sizeof(T), &bytes) ||
            !lfi_box_bufvalid(box_, reinterpret_cast<lfiptr>(raw), bytes)) {
            return {};
        }
        std::vector<std::remove_cv_t<T>> out(count);
        std::memcpy(out.data(), raw, bytes);
        return out;
    }

    // Memory mapping.

    void* mmap(void* addr, size_t length, int prot, int flags, int fd,
               off_t offset);
    int munmap(void* addr, size_t length);

    // Callbacks.

    // Register a host function that the sandbox can call. Pointer parameters
    // are not allowed, so callbacks that receive sandbox pointers must take
    // sbox<T*> and be registered with register_callback<fn>().
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
        return register_callback_raw(fn);
    }

    template<auto fn>
    auto register_callback() {
        static_assert(detail::callback_sandbox_matches_v<LFI, decltype(fn)>,
                      "Callback Sandbox parameter must match this backend");
        return register_callback_raw(
            &detail::callback_thunk_impl<LFI, decltype(fn), fn>::call);
    }

private:
    // Integral parameters narrower than 32 bits are rejected because the
    // sandbox controls the upper bits of each argument register and host
    // compilers may assume that narrow arguments are already extended.
    template<typename Ret, typename... Args>
    sbox<Ret (*)(Args...)> register_callback_raw(Ret (*fn)(Args...)) {
        static_assert((((!std::is_integral_v<Args> &&
                         !std::is_enum_v<Args>) ||
                        sizeof(Args) >= 4) && ...),
                      "LFI callback integer parameters must be at least 32 "
                      "bits wide");
        constexpr size_t n_int = ((detail::is_float_arg<Args> ? 0 : 1) + ... + 0);
        constexpr size_t n_float = ((detail::is_float_arg<Args> ? 1 : 0) + ... + 0);
        static_assert(n_int <= detail::max_int_reg_args && n_float <= detail::max_float_reg_args,
                      "LFI callbacks do not support stack arguments");
        static_assert((detail::is_reg_type<Args> && ...) &&
                          (std::is_void_v<Ret> || detail::is_reg_type<Ret>),
                      "LFI callback arguments and return values must be "
                      "scalars of at most 8 bytes");
        void* raw = lfi_box_register_cb(box_, reinterpret_cast<void*>(fn));
        if (!raw) {
            fprintf(stderr, "sbox: callback registration failed\n");
            abort();
        }
        return sbox<Ret (*)(Args...)>(
            reinterpret_cast<Ret (*)(Args...)>(raw));
    }

public:

    // Detach the current host thread from this sandbox and release its
    // sandbox context. Threads are otherwise detached when they exit, and
    // the memory of a destroyed sandbox is not released until every thread
    // that used it has detached or exited. No-op if the thread is not
    // attached. Aborts if a CallContext for this sandbox is live on the
    // thread or if the thread is inside a call into this sandbox, such as
    // in a callback.
    void detach_thread();

    // Context-aware calls (defined after CallContext).
    template<typename Sig, typename... Args>
    auto call(CallContext<LFI>& ctx, const char* name, Args... args);

    template<typename Ret, typename... Params, typename... Args>
    auto call(CallContext<LFI>& ctx, TypedName<Ret (*)(Params...)> tn,
              Args... args);

    CallContext<LFI> context();

    // Records a thunked callback as an active call while it runs, so that a
    // CallContext created in the callback is bound to it, and aborts if such
    // a context is still live when the callback returns, since its values
    // would remain on the sandbox stack the callback returns to.
    class CallbackScope {
        CallContext<LFI>* contexts_;
        std::optional<detail::ScopedSandboxTLS> guard_;

    public:
        CallbackScope();
        ~CallbackScope();
        CallbackScope(const CallbackScope&) = delete;
        CallbackScope& operator=(const CallbackScope&) = delete;
    };

    LFIBox* native_handle() const { return box_; }
    LFILinuxProc* proc() const { return proc_; }

private:
    friend class CallContext<LFI>;
    friend struct detail::ThreadCleanup;

    void check_range(const void* p, size_t n, const char* what) {
        if (!lfi_box_bufvalid(box_, reinterpret_cast<lfiptr>(p), n)) {
            fprintf(stderr,
                    "sbox: %s failed: [%p, +%zu) not in sandbox\n",
                    what, p, n);
            abort();
        }
    }

    // Stack allocation on this thread's context (used by CallContext).
    LFIContext* thread_ctx() { return *get_thread_ctx(); }
    void* stack_push(LFIContext* ctx, size_t size, size_t align = 16);

    // Detach an exiting host thread and erase its per-thread context entry.
    // Called from detail::ThreadCleanup::~ThreadCleanup.
    void detach_exiting_thread(uint64_t thread);

    std::vector<void*> idmem_allocations_;
    std::mutex idmem_mutex_;

    template<typename Ret, typename... Params, typename... Args>
    Ret call_with_sig_impl(void* fn, Ret (*)(Params...), Args... args) {
        return call_ptr<Ret, Params...>(
            fn, detail::convert_call_arg<Params>(args)...);
    }

    template<typename Sig, typename... Args>
    auto call_ptr_sig(void* fn, Args... args) {
        return call_with_sig_impl(fn, static_cast<Sig*>(nullptr), args...);
    }

    lfiptr lookup(const char* name);

    LFIContext** get_thread_ctx();
};

template<>
class CallContext<LFI> {
    friend class Sandbox<LFI>;

    Sandbox<LFI>* sandbox_;
    LFIContext* ctx_;
    uint64_t saved_sp_;

    // Sequence number of the innermost active sandbox call when the context
    // was created. A nested call moves the sandbox stack pointer, so the
    // context may only be used while the same call is innermost.
    uint64_t call_;

    // An out or inout value waiting to be copied back to the host.
    struct Copyback {
        void* host;
        const void* slot;
        size_t size;
        bool is_bool;
    };
    std::vector<Copyback> copybacks_;

    // Links in this thread's list of live contexts, newest first.
    CallContext* prev_ = nullptr;
    CallContext* next_ = nullptr;

    void check_nesting(const char* what) const {
        if (detail::current_call_seq() != call_) {
            fprintf(stderr, "sbox: CallContext %s inside a nested call\n",
                    what);
            abort();
        }
    }

    // Values live on the thread's sandbox stack, so only the newest live
    // context on a sandbox context may push, and contexts must be destroyed
    // in reverse order of creation.
    void check_newest(const char* what) const {
        check_nesting(what);
        for (auto* c = detail::tls_live_contexts; c != this; c = c->next_) {
            if (c->ctx_ == ctx_) {
                fprintf(stderr,
                        "sbox: CallContext %s while a newer one is live\n",
                        what);
                abort();
            }
        }
    }

    // Copy a value written by the sandbox to the host. A bool is normalized
    // because the sandbox may store any byte value in it.
    static void copy_back(const Copyback& c) {
        if (c.is_bool) {
            unsigned char byte;
            std::memcpy(&byte, c.slot, 1);
            bool value = byte != 0;
            std::memcpy(c.host, &value, sizeof(value));
        } else {
            std::memcpy(c.host, c.slot, c.size);
        }
    }

    template<typename T>
    void add_copyback(T* host_ptr, const T* slot) {
        copybacks_.push_back(
            {host_ptr, slot, sizeof(T),
             std::is_same_v<std::remove_cv_t<T>, bool>});
    }

    // Copy back and drop the out and inout values passed to a context call.
    template<typename... Args>
    void complete(const Args&... args) {
        (complete_arg(args), ...);
    }

    // Each value is copied back once, after the first context call that
    // receives it, so the host variable need not outlive the context.
    template<typename A>
    void complete_arg(const A& arg) {
        using P = detail::sbox_inner_type_t<A>;
        if constexpr (std::is_pointer_v<P> &&
                      !std::is_function_v<std::remove_pointer_t<P>>) {
            uintptr_t p;
            if constexpr (detail::is_sbox_safe_ptr_v<A>)
                p = reinterpret_cast<uintptr_t>(arg.data());
            else if constexpr (detail::is_sbox_ptr_v<A>)
                p = reinterpret_cast<uintptr_t>(arg.unsafe_unverified());
            else
                p = reinterpret_cast<uintptr_t>(arg);
            for (size_t i = 0; i < copybacks_.size(); i++) {
                if (reinterpret_cast<uintptr_t>(copybacks_[i].slot) == p) {
                    copy_back(copybacks_[i]);
                    copybacks_.erase(copybacks_.begin() + i);
                    return;
                }
            }
        }
    }

public:
    // A context belongs to the thread that creates it, and values pushed to
    // it live on that thread's sandbox stack until it is destroyed.
    explicit CallContext(Sandbox<LFI>& sb)
        : sandbox_(&sb),
          ctx_(sb.thread_ctx()),
          saved_sp_(detail::get_sp(lfi_ctx_regs(ctx_))),
          call_(detail::current_call_seq()) {
        next_ = detail::tls_live_contexts;
        if (next_)
            next_->prev_ = this;
        detail::tls_live_contexts = this;
    }

    ~CallContext() {
        check_newest("destroyed");
        for (auto& c : copybacks_)
            copy_back(c);
        detail::set_sp(lfi_ctx_regs(ctx_), saved_sp_);
        if (prev_)
            prev_->next_ = next_;
        else
            detail::tls_live_contexts = next_;
        if (next_)
            next_->prev_ = prev_;
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
        check_newest("used");
        T* sbox_ptr = static_cast<T*>(sandbox_->stack_push(ctx_, sizeof(T), alignof(T)));
        T* host_ptr = &host_ref;
        // The slot starts with the host value so that a callee that does
        // not write it leaves the host value unchanged.
        std::memcpy(sbox_ptr, &host_ref, sizeof(T));
        add_copyback(host_ptr, sbox_ptr);
        return detail::context_ptr_t<T>(sbox_ptr);
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
        check_newest("used");
        T* sbox_ptr = static_cast<T*>(sandbox_->stack_push(ctx_, sizeof(T), alignof(T)));
        std::memcpy(sbox_ptr, &host_ref, sizeof(T));
        return detail::context_ptr_t<const T>(sbox_ptr);
    }

    template<typename T>
    detail::context_ptr_t<T> inout(T& host_ref) {
        static_assert(!std::is_const_v<T>,
                      "inout requires a non-const variable");
        static_assert(detail::context_value_ok<T>,
                      "inout requires a trivially-copyable type that is not "
                      "a raw or sbox_safe pointer");
        check_newest("used");
        T* sbox_ptr = static_cast<T*>(sandbox_->stack_push(ctx_, sizeof(T), alignof(T)));
        T* host_ptr = &host_ref;
        std::memcpy(sbox_ptr, &host_ref, sizeof(T));
        add_copyback(host_ptr, sbox_ptr);
        return detail::context_ptr_t<T>(sbox_ptr);
    }
};

inline Sandbox<LFI>::CallbackScope::CallbackScope()
    : contexts_(detail::tls_live_contexts) {
    if (detail::tls_active_calls)
        guard_.emplace(detail::tls_active_calls->sandbox);
}

inline Sandbox<LFI>::CallbackScope::~CallbackScope() {
    guard_.reset();
    if (detail::tls_live_contexts != contexts_) {
        fprintf(stderr, "sbox: CallContext outlived the callback it was "
                        "created in\n");
        abort();
    }
}

template<typename Sig, typename... Args>
auto Sandbox<LFI>::call(CallContext<LFI>& ctx, const char* name,
                        Args... args) {
    if (ctx.sandbox_ != this) {
        fprintf(stderr, "sbox: CallContext used with another sandbox\n");
        abort();
    }
    ctx.check_nesting("used");
    using Ret = detail::sig_return_t<Sig>;
    if constexpr (std::is_void_v<Ret>) {
        this->template call<Sig>(name, args...);
        ctx.complete(args...);
    } else {
        auto result = this->template call<Sig>(name, args...);
        ctx.complete(args...);
        return result;
    }
}

template<typename Ret, typename... Params, typename... Args>
auto Sandbox<LFI>::call(CallContext<LFI>& ctx,
                        TypedName<Ret (*)(Params...)> tn, Args... args) {
    static_assert(sizeof...(Params) == sizeof...(Args),
                  "Wrong number of arguments for sandboxed function");
    static_assert(
        (detail::check_sbox_ptr_arg_v<Params, Args> && ...),
        "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
        "matching type, and parameters cannot be references");
    return call<Ret(Params...)>(ctx, tn.name, args...);
}

} // namespace sbox
