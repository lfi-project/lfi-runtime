#include "sbox/lfi.h"

#include <algorithm>
#include <cerrno>

namespace sbox {

detail::LiveSandboxes& detail::live_sandboxes() {
    // Allocated once and never freed so that threads exiting during static
    // destruction can still use it.
    static LiveSandboxes* live = new LiveSandboxes();
    return *live;
}

detail::ThreadCleanup::~ThreadCleanup() {
    // Sandbox calls made while detaching are not recorded for cleanup.
    tls_cleanup_done = true;
    last_sandbox_id = 0;
    last_ctxp = nullptr;
    std::vector<ThreadAttachment> pending = std::move(attachments);
    attachments.clear();

    auto& live = live_sandboxes();
    for (auto& a : pending) {
        bool alive;
        {
            std::lock_guard<std::mutex> lock(live.mutex);
            auto it = live.pins.find(a.sandbox_id);
            alive = it != live.pins.end();
            if (alive)
                it->second++;
        }
        if (!alive) {
            lfi_linux_detach_thread(a.proc);
            continue;
        }
        a.sandbox->detach_exiting_thread(a.thread);
        {
            std::lock_guard<std::mutex> lock(live.mutex);
            live.pins[a.sandbox_id]--;
        }
        live.unpinned.notify_all();
    }
}

bool LFIManager::init(size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engine_) {
        return false;
    }
    return create(n);
}

bool LFIManager::destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sandboxes_ > 0)
        return false;
    if (linux_engine_) {
        lfi_linux_free(linux_engine_);
        linux_engine_ = nullptr;
    }
    if (engine_) {
        lfi_free(engine_);
        engine_ = nullptr;
    }
    return true;
}


bool LFIManager::create(size_t n) {
    engine_ = lfi_new(
        {
            .pagesize = static_cast<size_t>(getpagesize()),
            .boxsize = 4ULL * 1024 * 1024 * 1024,
            .verbose = false,
            .stores_only = false,
            .no_verify = false,
            .allow_wx = false,
            .no_init_sigaltstack = false,
        },
        n);
    if (!engine_) {
        return false;
    }

    linux_engine_ = lfi_linux_new(engine_,
                                   {
                                       .stacksize = 2 * 1024 * 1024,
                                       .verbose = false,
                                       .perf = false,
                                       .exit_unknown_syscalls = false,
                                       .debug = false,
                                       .brk_control = false,
                                       .brk_size = 0,
                                   });
    if (!linux_engine_) {
        lfi_free(engine_);
        engine_ = nullptr;
        return false;
    }
    return true;
}

LFILinuxEngine* LFIManager::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!engine_ && !create(1))
        return nullptr;
    sandboxes_++;
    return linux_engine_;
}

void LFIManager::release() {
    std::lock_guard<std::mutex> lock(mutex_);
    sandboxes_--;
}

std::unique_ptr<Sandbox<LFI>> Sandbox<LFI>::create(const char* library_path) {
    std::unique_ptr<Sandbox<LFI>> sb(new Sandbox<LFI>());
    LFILinuxEngine* linux_engine = LFIManager::acquire();
    if (!linux_engine) {
        return nullptr;
    }
    sb->holds_engine_ = true;
    sb->id_ = detail::sandbox_id_counter.fetch_add(1, std::memory_order_relaxed);
    {
        auto& live = detail::live_sandboxes();
        std::lock_guard<std::mutex> lock(live.mutex);
        live.pins.emplace(sb->id_, 0);
    }

    sb->proc_ = lfi_proc_new(linux_engine);
    if (!sb->proc_) {
        return nullptr;
    }

    if (!lfi_proc_load_file(sb->proc_, library_path)) {
        return nullptr;
    }

    sb->box_ = lfi_proc_box(sb->proc_);
    // The library supplies _lfi_ret, so it must name an instruction inside
    // an executable mapping of the sandbox.
    lfiptr lfi_ret = lfi_proc_sym(sb->proc_, "_lfi_ret");
    LFIMapInfo info;
    if (!lfi_ret || !lfi_box_ptrvalid(sb->box_, lfi_ret) ||
        !lfi_box_mquery(sb->box_, lfi_ret, &info) ||
        !(info.prot & LFI_PROT_EXEC)) {
        return nullptr;
    }
#if defined(__aarch64__)
    if (lfi_ret % 4 != 0)
        return nullptr;
#endif
    lfi_box_register_ret(sb->box_, lfi_ret);

    const char* envp[] = {nullptr};
    const char* argv[] = {library_path, nullptr};
    sb->main_thread_ = lfi_thread_new(sb->proc_, 0, argv, envp);
    if (!sb->main_thread_) {
        return nullptr;
    }
    int run_result = lfi_thread_run(sb->main_thread_);
    if (run_result != 0) {
        return nullptr;
    }

    lfi_linux_init_clone(sb->main_thread_);
    sb->main_thread_token_ = detail::thread_token();

    return sb;
}

Sandbox<LFI>::~Sandbox() {
    {
        auto& live = detail::live_sandboxes();
        std::unique_lock<std::mutex> lock(live.mutex);
        live.unpinned.wait(lock, [&]() {
            auto it = live.pins.find(id_);
            return it == live.pins.end() || it->second == 0;
        });
        live.pins.erase(id_);
    }
    if (main_thread_)
        lfi_thread_free(main_thread_);
    if (proc_)
        lfi_proc_free(proc_);
    if (holds_engine_)
        LFIManager::release();
}

lfiptr Sandbox<LFI>::lookup(const char* name) {
    std::lock_guard<std::mutex> lock(symbol_cache_mutex_);

    auto it = symbol_cache_.find(name);
    if (it != symbol_cache_.end()) {
        return it->second;
    }

    lfiptr sym = lfi_proc_sym(proc_, name);
    if (!sym) {
        fprintf(stderr, "sbox: symbol not found: %s\n", name);
        abort();
    }
    symbol_cache_[name] = sym;
    return sym;
}

LFIContext** Sandbox<LFI>::get_thread_ctx() {
    // Fast path: same Sandbox as last call on this thread.
    if (detail::last_sandbox_id == id_ && *detail::last_ctxp)
        return detail::last_ctxp;

    // Slow path: look up (and lazily create) this thread's entry. Map nodes
    // are stable and only this thread modifies its own entry, so the context
    // is cloned without holding the lock in case the sandbox re-enters the
    // host during the clone.
    LFIContext** ctxp;
    {
        std::lock_guard<std::mutex> lock(thread_ctx_mutex_);
        uint64_t id = detail::thread_token();
        auto [it, inserted] = thread_ctxs_.try_emplace(id, nullptr);
        if (inserted) {
            if (main_thread_token_ == id) {
                it->second = *lfi_thread_ctxp(main_thread_);
            } else if (!detail::tls_cleanup_done) {
                detail::tls_cleanup.attachments.push_back(
                    {this, id_, id, proc_});
            }
        }
        ctxp = &it->second;
    }
    if (!*ctxp) {
        detail::ScopedSandboxTLS _tls_guard(this);
        lfi_clone(box_, ctxp);
        if (!*ctxp) {
            fprintf(stderr, "sbox: lfi_clone failed for worker thread\n");
            abort();
        }
    }

    detail::last_sandbox_id = id_;
    detail::last_ctxp = ctxp;
    return ctxp;
}

void* Sandbox<LFI>::stack_push(LFIContext* ctx, size_t size, size_t align) {
    LFIRegs* regs = lfi_ctx_regs(ctx);
    uint64_t sp = detail::get_sp(regs);
    // The stack pointer stays 16-byte aligned because the AArch64 trampoline
    // enters the sandbox without realigning it.
    align = std::max<size_t>(align, 16);
    sp = (sp - size) & ~(align - 1);
    check_range(reinterpret_cast<void*>(sp), size, "stack_push");
    detail::set_sp(regs, sp);
    return reinterpret_cast<void*>(sp);
}

// MAP_FIXED_NOREPLACE is Linux-specific.
#ifdef MAP_FIXED_NOREPLACE
static constexpr int map_fixed_noreplace = MAP_FIXED_NOREPLACE;
#else
static constexpr int map_fixed_noreplace = 0;
#endif

void* Sandbox<LFI>::mmap(void* addr, size_t length, int prot, int flags,
                          int fd, off_t offset) {
    // Executable mappings are refused because a file mapping can alias a
    // writable mapping of the same file, which would let code run without
    // verification. Flags without an LFI equivalent are refused as well.
    constexpr int supported = MAP_SHARED | MAP_PRIVATE | MAP_FIXED |
                              map_fixed_noreplace | MAP_ANONYMOUS;
    if ((prot & PROT_EXEC) || (flags & ~supported)) {
        errno = EINVAL;
        return MAP_FAILED;
    }
    int lfi_prot = 0;
    if (prot & PROT_READ) lfi_prot |= LFI_PROT_READ;
    if (prot & PROT_WRITE) lfi_prot |= LFI_PROT_WRITE;

    int lfi_flags = 0;
    if (flags & MAP_SHARED) lfi_flags |= LFI_MAP_SHARED;
    if (flags & MAP_PRIVATE) lfi_flags |= LFI_MAP_PRIVATE;
    if (flags & MAP_FIXED) lfi_flags |= LFI_MAP_FIXED;
    if (flags & MAP_ANONYMOUS) lfi_flags |= LFI_MAP_ANONYMOUS;

    // Fixed mappings cannot replace mappings the runtime owns.
    lfiptr result;
    lfiptr target = reinterpret_cast<lfiptr>(addr);
    if (flags & map_fixed_noreplace) {
        result = lfi_box_mapat_noreplace(box_, target, length, lfi_prot,
                                         lfi_flags | LFI_MAP_FIXED, fd,
                                         offset);
    } else if (flags & MAP_FIXED) {
        result = lfi_box_mapat_unpinned(box_, target, length, lfi_prot,
                                        lfi_flags, fd, offset);
    } else {
        result = lfi_box_mapany(box_, length, lfi_prot, lfi_flags, fd, offset);
    }
    if (result == static_cast<lfiptr>(-1)) {
        if (flags & map_fixed_noreplace)
            errno = EEXIST;
        else
            errno = (flags & MAP_FIXED) ? EINVAL : ENOMEM;
        return MAP_FAILED;
    }
    return reinterpret_cast<void*>(result);
}

int Sandbox<LFI>::munmap(void* addr, size_t length) {
    // Mappings the runtime owns, such as thread stacks and the callback
    // table, cannot be unmapped.
    int r = lfi_box_munmap_unpinned(box_, reinterpret_cast<lfiptr>(addr),
                                    length);
    if (r < 0) {
        errno = r == -1 ? EINVAL : -r;
        return -1;
    }
    return 0;
}



void Sandbox<LFI>::idmem_reset() {
    // The sandbox's free runs without the lock held, since it may invoke
    // callbacks that use idmem.
    std::vector<void*> pending;
    {
        std::lock_guard<std::mutex> lock(idmem_mutex_);
        pending.swap(idmem_allocations_);
    }
    detail::ScopedSandboxTLS _tls_guard(this);
    for (void* p : pending) {
        lfi_lib_free(box_, get_thread_ctx(), p);
    }
}

sbox_safe<char*> Sandbox<LFI>::copy_string(const char* s) {
    size_t len = std::strlen(s) + 1;
    auto buf = alloc<char>(len);
    if (!buf)
        return {};
    copy_to(buf, s, len);
    return buf;
}

CallContext<LFI> Sandbox<LFI>::context() {
    return CallContext<LFI>(*this);
}

void Sandbox<LFI>::detach_exiting_thread(uint64_t thread) {
    {
        detail::ScopedSandboxTLS _tls_guard(this);
        lfi_linux_detach_thread(proc_);
    }
    {
        std::lock_guard<std::mutex> lock(thread_ctx_mutex_);
        thread_ctxs_.erase(thread);
    }
    if (detail::last_sandbox_id == id_) {
        detail::last_sandbox_id = 0;
        detail::last_ctxp = nullptr;
    }
}

void Sandbox<LFI>::detach_thread() {
    if (detail::in_sandbox_call(this)) {
        fprintf(stderr,
                "sbox: detach_thread called during a call into the sandbox\n");
        abort();
    }
    for (auto* c = detail::tls_live_contexts; c; c = c->next_) {
        if (c->sandbox_ == this) {
            fprintf(stderr,
                    "sbox: detach_thread called with a live CallContext\n");
            abort();
        }
    }
    {
        detail::ScopedSandboxTLS _tls_guard(this);
        lfi_linux_detach_thread(proc_);
    }
    {
        std::lock_guard<std::mutex> lock(thread_ctx_mutex_);
        thread_ctxs_.erase(detail::thread_token());
    }
    if (!detail::tls_cleanup_done) {
        auto& a = detail::tls_cleanup.attachments;
        a.erase(std::remove_if(a.begin(), a.end(),
                    [this](const detail::ThreadAttachment& x) {
                        return x.sandbox_id == id_;
                    }), a.end());
    }
    if (detail::last_sandbox_id == id_) {
        detail::last_sandbox_id = 0;
        detail::last_ctxp = nullptr;
    }
}

}  // namespace sbox
