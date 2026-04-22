#pragma once

#include <cstddef>
#include <cstring>
#include <mutex>
#include <type_traits>
#include <unordered_map>

namespace sbox {

// Forward declarations for sandbox pointer types
template<typename T>
class sbox;

template<typename T>
class sbox_safe;

// sbox<T*> - unchecked sandbox pointer.
// Returned by sandbox function calls and passed to callbacks.
// Cannot be dereferenced directly; must be verified or explicitly unwrapped.
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
    T* ptr_;

public:
    sbox_safe() : ptr_(nullptr) {}
    explicit sbox_safe(T* p) : ptr_(p) {}

    T& operator*() const { return *ptr_; }
    T& operator[](size_t i) const { return ptr_[i]; }
    T* data() const { return ptr_; }

    // Implicit promotion to unchecked sbox<T*> (always safe — dropping trust)
    operator sbox<T*>() const { return sbox<T*>(ptr_); }

    explicit operator bool() const { return ptr_ != nullptr; }
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr_ != nullptr; }
};

namespace detail {

// Type traits for sbox pointer types
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

// Extract the inner pointer type from sbox<T*> or sbox_safe<T*>
template<typename T>
struct sbox_inner_type { using type = T; };
template<typename T>
struct sbox_inner_type<sbox<T*>> { using type = T*; };
template<typename T>
struct sbox_inner_type<sbox_safe<T*>> { using type = T*; };
template<typename T>
using sbox_inner_type_t =
    typename sbox_inner_type<std::remove_cv_t<std::remove_reference_t<T>>>::type;

// Check that pointer parameters use sbox types with matching inner types.
template<typename Param, typename Arg>
constexpr bool check_sbox_ptr_arg() {
    if constexpr (!std::is_pointer_v<Param>) {
        return true;
    } else if constexpr (!is_sbox_ptr_v<Arg> && !is_sbox_safe_ptr_v<Arg>) {
        return false;
    } else {
        using Inner = sbox_inner_type_t<Arg>;
        if constexpr (std::is_function_v<std::remove_pointer_t<Param>>) {
            // Function pointer: exact type match required
            return std::is_same_v<Inner, Param>;
        } else {
            // Data pointer: cv-lenient match
            using P = std::remove_cv_t<std::remove_pointer_t<Param>>;
            using A = std::remove_cv_t<std::remove_pointer_t<Inner>>;
            return std::is_same_v<P, A>;
        }
    }
}

template<typename Param, typename Arg>
inline constexpr bool check_sbox_ptr_arg_v = check_sbox_ptr_arg<Param, Arg>();

// Unwrap sbox types to raw pointers (for passing to sandbox)
template<typename T>
auto unwrap_sbox_arg(T val) {
    if constexpr (is_sbox_ptr_v<T>) {
        return val.unsafe_unverified();
    } else if constexpr (is_sbox_safe_ptr_v<T>) {
        return val.data();
    } else {
        return val;
    }
}

// Convert a call argument to the target parameter type, unwrapping sbox types
// and using reinterpret_cast for pointer conversions.
template<typename To, typename From>
To convert_call_arg(From arg) {
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

// Wrap raw pointer returns in sbox<T*>
template<typename T>
auto wrap_sbox_return(T val) {
    if constexpr (std::is_pointer_v<T>) {
        return sbox<T>(val);
    } else {
        return val;
    }
}

// Unwrap sbox<T*> to T* at the type level (for callback thunks)
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

// Extract return type from function signature
template<typename Sig>
struct sig_return;
template<typename Ret, typename... Args>
struct sig_return<Ret(Args...)> {
    using type = Ret;
};
template<typename Sig>
using sig_return_t = typename sig_return<Sig>::type;

// Callback thunk: wraps raw pointer args in sbox<T*> before calling the
// user's callback function. Used by passthrough register_callback to bridge
// between the C library (which passes raw pointers) and the user's callback
// (which expects sbox<T*> arguments).
template<typename FnPtr, FnPtr fn>
struct callback_thunk_impl;

template<typename Ret, typename... Args, Ret (*fn)(Args...)>
struct callback_thunk_impl<Ret (*)(Args...), fn> {
    using c_type = Ret (*)(unwrap_sbox_type_t<Args>...);
    static Ret call(unwrap_sbox_type_t<Args>... raw_args) {
        if constexpr (std::is_void_v<Ret>) {
            fn(Args(raw_args)...);
        } else {
            return fn(Args(raw_args)...);
        }
    }
};

// Thread-local sandbox pointer, set during sandbox calls so that callback
// thunks can inject the sandbox reference into user callbacks.
inline thread_local void* tls_current_sandbox = nullptr;

}  // namespace detail

// TypedName - carries a function's name string along with its declared type.
// Created by the SBOX_FN macro in dynamic mode so that call-site type
// checking can be performed against the library header declaration.
template<typename FnPtr>
struct TypedName {
    const char* name;
    // Implicit conversion to const char* for backward compatibility
    // with call<Sig>(const char*, ...) overloads.
    operator const char*() const { return name; }
};

}  // namespace sbox

// Macro to reference a function - expands differently based on backend.
// In static mode: expands to function pointer (direct call, type-safe).
// In dynamic mode: expands to TypedName carrying name + type from declaration.
#ifdef SBOX_STATIC
#define SBOX_FN(name) (name)
#else
#define SBOX_FN(name) (sbox::TypedName<decltype(&name)>{#name})
#endif

namespace sbox {

// Backend tag types
struct Passthrough {};
struct Process {};
struct LFI {};

// Forward declarations
template<typename Backend>
class Sandbox;

template<typename Backend>
class CallContext;

// Specialization for callbacks whose first parameter is Sandbox<Backend>&.
// The thunk strips the sandbox parameter from the C-visible signature and
// injects it from thread-local storage at call time.
namespace detail {
template<typename Backend, typename Ret, typename... Args,
         Ret (*fn)(Sandbox<Backend>&, Args...)>
struct callback_thunk_impl<Ret (*)(Sandbox<Backend>&, Args...), fn> {
    using c_type = Ret (*)(unwrap_sbox_type_t<Args>...);
    static Ret call(unwrap_sbox_type_t<Args>... raw_args) {
        auto& sandbox =
            *static_cast<Sandbox<Backend>*>(tls_current_sandbox);
        if constexpr (std::is_void_v<Ret>) {
            fn(sandbox, Args(raw_args)...);
        } else {
            return fn(sandbox, Args(raw_args)...);
        }
    }
};
}  // namespace detail

// Function handle - captures sandbox reference for direct calls
template<typename Backend, typename Sig>
class FnHandle;

template<typename Backend, typename Ret, typename... Args>
class FnHandle<Backend, Ret(Args...)> {
public:
    FnHandle(Sandbox<Backend>& sandbox, void* fn_ptr)
        : sandbox_(&sandbox), fn_ptr_(fn_ptr) {}

    template<typename... CallArgs>
    auto operator()(CallArgs... args) const {
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

}  // namespace sbox
