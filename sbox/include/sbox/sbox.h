#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <unordered_map>

namespace sbox {

template<typename T>
class sbox;

template<typename T>
class sbox_safe;

template<typename Backend>
class Sandbox;

namespace detail {

// Whether a type is or points to an sbox_safe pointer, directly or through
// sbox pointers, raw pointers, or arrays. Such types cannot describe sandbox
// memory, because a value the sandbox writes there would appear verified.
template<typename T>
struct holds_sbox_safe : std::false_type {};
template<typename T>
struct holds_sbox_safe<sbox_safe<T*>> : std::true_type {};
template<typename T>
struct holds_sbox_safe<sbox<T*>> : holds_sbox_safe<std::remove_cv_t<T>> {};
template<typename T>
struct holds_sbox_safe<T*> : holds_sbox_safe<std::remove_cv_t<T>> {};
template<typename T, size_t N>
struct holds_sbox_safe<T[N]> : holds_sbox_safe<std::remove_cv_t<T>> {};
template<typename T>
inline constexpr bool holds_sbox_safe_v =
    holds_sbox_safe<std::remove_cv_t<T>>::value;

// Whether a type is a raw pointer, a member pointer, or an array of them.
template<typename T>
struct holds_raw_ptr
    : std::bool_constant<std::is_pointer_v<T> || std::is_member_pointer_v<T>> {
};
template<typename T, size_t N>
struct holds_raw_ptr<T[N]> : holds_raw_ptr<std::remove_cv_t<T>> {};
template<typename T>
inline constexpr bool holds_raw_ptr_v =
    holds_raw_ptr<std::remove_cv_t<T>>::value;

// Whether a type is bool or an array of them. The sandbox may store any byte
// in a bool, which is not a valid bool value for the host.
template<typename T>
struct holds_bool : std::is_same<T, bool> {};
template<typename T, size_t N>
struct holds_bool<T[N]> : holds_bool<std::remove_cv_t<T>> {};
template<typename T>
inline constexpr bool holds_bool_v = holds_bool<std::remove_cv_t<T>>::value;

// Whether an enum type is defined for every value of its underlying type.
// That holds for scoped enums, whose underlying type is always fixed, unless
// the underlying type is bool. Unscoped enums are rejected because C++ cannot
// tell whether their underlying type is fixed, and the sandbox could supply
// a value outside their range.
template<typename T>
constexpr bool enum_value_ok() {
    if constexpr (std::is_enum_v<T>) {
        using U = std::underlying_type_t<T>;
        return !std::is_convertible_v<T, U> && !std::is_same_v<U, bool>;
    } else {
        return true;
    }
}

// Whether a type is an enum that the sandbox may not supply, or an array of
// them.
template<typename T>
struct holds_unsafe_enum : std::bool_constant<!enum_value_ok<T>()> {};
template<typename T, size_t N>
struct holds_unsafe_enum<T[N]> : holds_unsafe_enum<std::remove_cv_t<T>> {};
template<typename T>
inline constexpr bool holds_unsafe_enum_v =
    holds_unsafe_enum<std::remove_cv_t<T>>::value;

// Whether every type is safe for the sandbox to supply as a callback
// argument.
template<typename... Args>
inline constexpr bool callback_enums_ok =
    (!holds_unsafe_enum_v<Args> && ...);

} // namespace detail

// sbox<T*> - unchecked sandbox pointer.
//
// Returned by sandbox function calls and passed to callbacks. Cannot be
// dereferenced directly and must be verified or explicitly unwrapped.
template<typename T>
class sbox<T*> {
    T* ptr_;

public:
    sbox() : ptr_(nullptr) {}
    explicit sbox(T* p) : ptr_(p) {}

    T* unsafe_unverified() const { return ptr_; }

    explicit operator bool() const { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr_ != nullptr; }
};

// sbox_safe<T*> - verified sandbox pointer, freely dereferenceable.
// Returned by alloc() or promoted from sbox<T*> via verify().
// Verified at creation time, not on each access.
template<typename T>
class sbox_safe<T*> {
    static_assert(!detail::holds_sbox_safe_v<T>,
                  "sbox_safe cannot be stored in sandbox memory");
    // Pointers in sandbox memory are written by the sandbox, so a verified
    // buffer holds them as sbox<U*>.
    static_assert(!detail::holds_raw_ptr_v<T>,
                  "Pointers in sandbox memory must be held as sbox<T*>");
    // A polymorphic object in sandbox memory has a vtable pointer that the
    // sandbox controls.
    static_assert(!std::is_polymorphic_v<T>,
                  "Polymorphic types cannot be stored in sandbox memory");
    static_assert(std::is_trivially_copyable_v<T>,
                  "Sandbox memory can only hold trivially-copyable types");
    static_assert(!detail::holds_bool_v<T>,
                  "bool values in sandbox memory must be accessed as "
                  "unsigned char");
    static_assert(!detail::holds_unsafe_enum_v<T>,
                  "Enums in sandbox memory must be scoped with a non-bool "
                  "underlying type");

    T* ptr_;

public:
    sbox_safe() : ptr_(nullptr) {}
    explicit sbox_safe(T* p) : ptr_(p) {}

    T& operator*() const { return *ptr_; }
    T& operator[](size_t i) const { return ptr_[i]; }
    T* data() const { return ptr_; }

    // Implicit promotion to unchecked sbox<T*>.
    operator sbox<T*>() const { return sbox<T*>(ptr_); }

    explicit operator bool() const { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr_ != nullptr; }
};

namespace detail {

// Type traits for sbox pointer types.
template<typename T>
struct is_sbox_ptr : std::false_type {};
template<typename T>
struct is_sbox_ptr<sbox<T*>> : std::true_type {};
template<typename T>
inline constexpr bool is_sbox_ptr_v =
    is_sbox_ptr<std::remove_cv_t<std::remove_reference_t<T>>>::value;

template<typename T>
struct is_sbox_safe_ptr : std::false_type {};
template<typename T>
struct is_sbox_safe_ptr<sbox_safe<T*>> : std::true_type {};
template<typename T>
inline constexpr bool is_sbox_safe_ptr_v =
    is_sbox_safe_ptr<std::remove_cv_t<std::remove_reference_t<T>>>::value;

// Extract the inner pointer type from sbox<T*> or sbox_safe<T*>.
template<typename T>
struct sbox_inner_type { using type = T; };
template<typename T>
struct sbox_inner_type<sbox<T*>> { using type = T*; };
template<typename T>
struct sbox_inner_type<sbox_safe<T*>> { using type = T*; };
template<typename T>
using sbox_inner_type_t =
    typename sbox_inner_type<std::remove_cv_t<std::remove_reference_t<T>>>::type;

inline bool checked_mul(size_t count, size_t size, size_t* out) {
    return !__builtin_mul_overflow(count, size, out);
}

template<typename T>
struct unwrap_sbox_type;

// Whether a pointer to From converts to a pointer to To by adding
// qualifiers only.
template<typename From, typename To>
inline constexpr bool adds_qualifiers =
    (!std::is_const_v<From> || std::is_const_v<To>) &&
    (!std::is_volatile_v<From> || std::is_volatile_v<To>);

// Check that pointer parameters use sbox types with matching inner types.
// A pointer to sbox<U*> matches a U** parameter, so values the sandbox
// writes through it stay tainted. Reference parameters are rejected.
template<typename Param, typename Arg>
constexpr bool check_sbox_ptr_arg() {
    if constexpr (std::is_reference_v<Param>) {
        return false;
    } else if constexpr (!std::is_pointer_v<Param>) {
        return true;
    } else if constexpr (!is_sbox_ptr_v<Arg> && !is_sbox_safe_ptr_v<Arg>) {
        return false;
    } else {
        using Inner = sbox_inner_type_t<Arg>;
        if constexpr (std::is_function_v<std::remove_pointer_t<Param>>) {
            return std::is_same_v<Inner, Param>;
        } else {
            using P = std::remove_pointer_t<Param>;
            using A = std::remove_pointer_t<Inner>;
            using AU = typename unwrap_sbox_type<std::remove_cv_t<A>>::type;
            return (std::is_same_v<std::remove_cv_t<P>, std::remove_cv_t<A>> ||
                    (is_sbox_ptr_v<A> &&
                     std::is_same_v<std::remove_cv_t<P>, AU>)) &&
                   adds_qualifiers<A, P>;
        }
    }
}

template<typename Param, typename Arg>
inline constexpr bool check_sbox_ptr_arg_v = check_sbox_ptr_arg<Param, Arg>();

// Types that hold values read from or written by the sandbox. Pointers must
// be held as sbox<T*> so that values the sandbox writes stay tainted.
template<typename T>
inline constexpr bool sandbox_value_ok =
    std::is_trivially_copyable_v<T> && !holds_raw_ptr_v<T> &&
    !holds_sbox_safe_v<T> && !holds_bool_v<T> && !holds_unsafe_enum_v<T>;

// CallContext also accepts a top-level bool, which it normalizes when
// copying back.
template<typename T>
inline constexpr bool context_value_ok =
    !std::is_volatile_v<T> &&
    (std::is_same_v<std::remove_cv_t<T>, bool> || sandbox_value_ok<T>);

// Pointer type returned by the CallContext helpers. A bool slot is returned
// as sbox<T*>, since sbox_safe cannot refer to bool.
template<typename T>
using context_ptr_t =
    std::conditional_t<std::is_same_v<std::remove_cv_t<T>, bool>, sbox<T*>,
                       sbox_safe<T*>>;

// Check the argument count and pointer arguments of a call against Sig.
template<typename Sig, typename... Args>
struct sig_args_check;
template<typename Ret, typename... Params, typename... Args>
struct sig_args_check<Ret(Params...), Args...> {
    static constexpr bool count_ok = sizeof...(Params) == sizeof...(Args);
    static constexpr bool ptrs_ok() {
        if constexpr (count_ok) {
            return (check_sbox_ptr_arg_v<Params, Args> && ... && true);
        } else {
            return true;
        }
    }
};

// Parameters of callbacks registered without a thunk must be plain values.
template<typename... Args>
inline constexpr bool no_raw_ptr_params =
    ((!std::is_pointer_v<Args> && !std::is_reference_v<Args> &&
      !is_sbox_ptr_v<Args> && !is_sbox_safe_ptr_v<Args>) && ...);

// Parameters of thunked callbacks that receive sandbox pointers must be
// sbox<T*>.
template<typename... Args>
inline constexpr bool thunk_params_ok =
    ((!std::is_pointer_v<Args> && !std::is_reference_v<Args> &&
      !holds_sbox_safe_v<Args>) && ...);

// Convert a call argument to the target parameter type, unwrapping sbox types
// and using reinterpret_cast for pointer conversions.
template<typename To, typename From>
To convert_call_arg(From arg) {
    static_assert(!std::is_reference_v<To>,
                  "Sandbox function parameters cannot be references");
    if constexpr (is_sbox_ptr_v<From>) {
        return reinterpret_cast<To>(arg.unsafe_unverified());
    } else if constexpr (is_sbox_safe_ptr_v<From>) {
        return reinterpret_cast<To>(arg.data());
    } else if constexpr (std::is_pointer_v<To> && std::is_pointer_v<From>) {
        return reinterpret_cast<To>(arg);
    } else {
        return static_cast<To>(arg);
    }
}

// Wrap raw pointer returns in sbox<T*>.
template<typename T>
auto wrap_sbox_return(T val) {
    static_assert(!holds_unsafe_enum_v<T>,
                  "Enum return types must be scoped with a non-bool "
                  "underlying type");
    if constexpr (std::is_pointer_v<T>) {
        return sbox<std::remove_cv_t<T>>(val);
    } else {
        return val;
    }
}

// Unwrap a callback's sbox<T*> or sbox_safe<T*> return value to T*.
template<typename T>
struct unwrap_return {
    using type = T;
};
template<typename T>
struct unwrap_return<sbox<T*>> {
    using type = T*;
};
template<typename T>
struct unwrap_return<sbox_safe<T*>> {
    using type = T*;
};
template<typename T>
using unwrap_return_t = typename unwrap_return<std::remove_cv_t<T>>::type;

template<typename T>
unwrap_return_t<T> unwrap_return_value(T val) {
    if constexpr (is_sbox_ptr_v<T>) {
        return val.unsafe_unverified();
    } else if constexpr (is_sbox_safe_ptr_v<T>) {
        return val.data();
    } else {
        return val;
    }
}

// Unwrap sbox<T*> to T* at the type level (for callback thunks).
template<typename T>
struct unwrap_sbox_type {
    using type = T;
};
template<typename T>
struct unwrap_sbox_type<sbox<T*>> {
    using type = T*;
};
template<typename T>
using unwrap_sbox_type_t = typename unwrap_sbox_type<T>::type;

// Remove noexcept from a function or function pointer type.
template<typename T>
struct strip_noexcept {
    using type = T;
};
template<typename Ret, typename... Args>
struct strip_noexcept<Ret(Args...) noexcept> {
    using type = Ret(Args...);
};
template<typename Ret, typename... Args>
struct strip_noexcept<Ret (*)(Args...) noexcept> {
    using type = Ret (*)(Args...);
};
template<typename T>
using strip_noexcept_t = typename strip_noexcept<T>::type;

// Extract return type from function signature.
template<typename Sig>
struct sig_return;
template<typename Ret, typename... Args>
struct sig_return<Ret(Args...)> {
    using type = Ret;
};
template<typename Sig>
using sig_return_t = typename sig_return<Sig>::type;

// Callback thunk: wraps raw pointer args in sbox<T*> before calling the
// user's callback function. Used by register_callback<fn>() on both backends
// to bridge between the C library (which passes raw pointers) and the user's
// callback (which expects sbox<T*> arguments).
template<typename Backend, typename FnPtr, FnPtr fn>
struct callback_thunk_impl;

template<typename Backend, typename Ret, typename... Args,
         Ret (*fn)(Args...)>
struct callback_thunk_impl<Backend, Ret (*)(Args...), fn> {
    static_assert(thunk_params_ok<Args...>,
                  "Callback pointer and reference parameters must be "
                  "sbox<T*>");
    static_assert(callback_enums_ok<Args...>,
                  "Callback enum parameters must be scoped with a non-bool "
                  "underlying type");
    using c_type = unwrap_return_t<Ret> (*)(unwrap_sbox_type_t<Args>...);
    static unwrap_return_t<Ret> call(unwrap_sbox_type_t<Args>... raw_args) {
        [[maybe_unused]] typename Sandbox<Backend>::CallbackScope scope;
        if constexpr (std::is_void_v<Ret>) {
            fn(Args(raw_args)...);
        } else {
            return unwrap_return_value(fn(Args(raw_args)...));
        }
    }
};

struct ScopedSandboxTLS;

// Innermost host call into a sandbox on this thread. Each call links to the
// call that encloses it, so the list holds every sandbox this thread is
// currently executing in.
inline thread_local ScopedSandboxTLS* tls_active_calls = nullptr;

// RAII guard that records a call into a sandbox for the lifetime of the
// enclosing scope. Callback thunks use the innermost call to inject the
// sandbox reference into user callbacks.
// Number of calls this thread has made into sandboxes, used to identify
// each call uniquely.
inline thread_local uint64_t tls_call_count = 0;

struct ScopedSandboxTLS {
    void* sandbox;
    ScopedSandboxTLS* outer;
    uint64_t seq;
    explicit ScopedSandboxTLS(void* sb) noexcept
        : sandbox(sb), outer(tls_active_calls), seq(++tls_call_count) {
        tls_active_calls = this;
    }
    ~ScopedSandboxTLS() noexcept { tls_active_calls = outer; }
    ScopedSandboxTLS(const ScopedSandboxTLS&) = delete;
    ScopedSandboxTLS& operator=(const ScopedSandboxTLS&) = delete;
};

// Sequence number of the innermost active call, or 0 if there is none.
inline uint64_t current_call_seq() {
    return tls_active_calls ? tls_active_calls->seq : 0;
}

// Whether this thread is currently executing in the given sandbox.
inline bool in_sandbox_call(const void* sb) {
    for (auto* c = tls_active_calls; c; c = c->outer) {
        if (c->sandbox == sb)
            return true;
    }
    return false;
}

} // namespace detail

// TypedName carries a function's name string along with its declared type.
// Created by the SBOX_FN macro in dynamic mode so that call-site type checking
// can be performed against the library header declaration.
template<typename FnPtr>
struct TypedName {
    const char* name;
};

}  // namespace sbox

// Macro to reference a function. Expands differently based on backend.
// static mode: expands to function pointer.
// dynamic mode: expands to TypedName carrying name + type from declaration.
#ifdef SBOX_STATIC
#define SBOX_FN(name) (name)
#else
#define SBOX_FN(name) \
    (sbox::TypedName<sbox::detail::strip_noexcept_t<decltype(&name)>>{#name})
#endif

namespace sbox {

struct Passthrough {};
struct LFI {};

template<typename Backend>
class Sandbox;

template<typename Backend>
class CallContext;

// Specialization for callbacks whose first parameter is Sandbox<Backend>&.
// The thunk strips the sandbox parameter from the C-visible signature and
// injects it from thread-local storage at call time.
namespace detail {
// Whether a callback's optional leading Sandbox parameter uses Backend.
template<typename Backend, typename FnPtr>
struct callback_sandbox_matches : std::true_type {};
template<typename Backend, typename Other, typename Ret, typename... Args>
struct callback_sandbox_matches<Backend, Ret (*)(Sandbox<Other>&, Args...)>
    : std::is_same<Backend, Other> {};
template<typename Backend, typename FnPtr>
inline constexpr bool callback_sandbox_matches_v =
    callback_sandbox_matches<Backend, FnPtr>::value;

template<typename Backend, typename Ret, typename... Args,
         Ret (*fn)(Sandbox<Backend>&, Args...)>
struct callback_thunk_impl<Backend, Ret (*)(Sandbox<Backend>&, Args...), fn> {
    static_assert(thunk_params_ok<Args...>,
                  "Callback pointer and reference parameters must be "
                  "sbox<T*>");
    static_assert(callback_enums_ok<Args...>,
                  "Callback enum parameters must be scoped with a non-bool "
                  "underlying type");
    using c_type = unwrap_return_t<Ret> (*)(unwrap_sbox_type_t<Args>...);
    // The sandbox is the one the host most recently called into on this
    // thread, so the callback must be invoked during a host call into a
    // sandbox.
    static unwrap_return_t<Ret> call(unwrap_sbox_type_t<Args>... raw_args) {
        if (!tls_active_calls) {
            fprintf(stderr, "sbox: callback invoked outside a sandbox call\n");
            abort();
        }
        auto& sandbox =
            *static_cast<Sandbox<Backend>*>(tls_active_calls->sandbox);
        [[maybe_unused]] typename Sandbox<Backend>::CallbackScope scope;
        if constexpr (std::is_void_v<Ret>) {
            fn(sandbox, Args(raw_args)...);
        } else {
            return unwrap_return_value(fn(sandbox, Args(raw_args)...));
        }
    }
};
} // namespace detail

// Function handle: captures sandbox reference for direct calls
template<typename Backend, typename Sig>
class FnHandle;

template<typename Backend, typename Ret, typename... Args>
class FnHandle<Backend, Ret(Args...)> {
public:
    FnHandle(Sandbox<Backend>& sandbox, void* fn_ptr)
        : sandbox_(&sandbox), fn_ptr_(fn_ptr) {}

    template<typename... CallArgs>
    auto operator()(CallArgs... args) const {
        static_assert(sizeof...(Args) == sizeof...(CallArgs),
                      "Wrong number of arguments for sandboxed function");
        static_assert(
            (detail::check_sbox_ptr_arg_v<Args, CallArgs> && ...),
            "Pointer arguments must be sbox<T*> or sbox_safe<T*> with a "
            "matching type, and parameters cannot be references");
        if constexpr (std::is_void_v<Ret>) {
            sandbox_->template call_ptr<Ret, Args...>(
                fn_ptr_, detail::convert_call_arg<Args>(args)...);
        } else {
            return detail::wrap_sbox_return(
                sandbox_->template call_ptr<Ret, Args...>(
                    fn_ptr_, detail::convert_call_arg<Args>(args)...));
        }
    }

private:
    Sandbox<Backend>* sandbox_;
    void* fn_ptr_;
};

} // namespace sbox
