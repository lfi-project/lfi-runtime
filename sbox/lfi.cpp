#include "sbox/lfi.h"

namespace sbox {

// -- LFIManager --

bool LFIManager::init(size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (engine_) {
        return false;
    }
    return create(n);
}

void LFIManager::destroy() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (linux_engine_) {
        lfi_linux_free(linux_engine_);
        linux_engine_ = nullptr;
    }
    if (engine_) {
        lfi_free(engine_);
        engine_ = nullptr;
    }
}

bool LFIManager::ensure() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!engine_) {
        return create(1);
    }
    return true;
}

bool LFIManager::create(size_t n) {
    const char* dir_maps[] = {nullptr};

    engine_ = lfi_new(
        {
            .pagesize = static_cast<size_t>(getpagesize()),
            .boxsize = 4ULL * 1024 * 1024 * 1024,
            .verbose = false,
            .stores_only = false,
            .no_verify = false,
            .allow_wx = false,
            .no_init_sigaltstack = false,
            .no_rtcall_nullpage = false,
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
                                       .dir_maps = dir_maps,
                                       .wd = nullptr,
                                       .exit_unknown_syscalls = false,
                                       .sys_passthrough = false,
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

LFILinuxEngine* LFIManager::get() {
    if (!ensure()) {
        return nullptr;
    }
    return linux_engine_;
}

// -- Sandbox<LFI> --

std::unique_ptr<Sandbox<LFI>> Sandbox<LFI>::create(const char* library_path) {
    LFILinuxEngine* linux_engine = LFIManager::get();
    if (!linux_engine) {
        return nullptr;
    }

    std::unique_ptr<Sandbox<LFI>> sb(new Sandbox<LFI>());

    sb->proc_ = lfi_proc_new(linux_engine);
    if (!sb->proc_) {
        return nullptr;
    }

    if (!lfi_proc_load_file(sb->proc_, library_path)) {
        return nullptr;
    }

    sb->box_ = lfi_proc_box(sb->proc_);
    lfiptr lfi_ret = lfi_proc_sym(sb->proc_, "_lfi_ret");
    if (!lfi_ret) {
        return nullptr;
    }
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
    sb->main_thread_tid_ = std::this_thread::get_id();

    return sb;
}

Sandbox<LFI>::~Sandbox() {
    if (main_thread_)
        lfi_thread_free(main_thread_);
    if (proc_)
        lfi_proc_free(proc_);
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
    static thread_local std::deque<ThreadCtxEntry> entries;

    for (auto& e : entries) {
        if (e.box == box_) return &e.ctx;
    }
    entries.push_back({box_, nullptr});
    auto& e = entries.back();
    if (main_thread_tid_ == std::this_thread::get_id()) {
        e.ctx = *lfi_thread_ctxp(main_thread_);
    }
    return &e.ctx;
}

void* Sandbox<LFI>::stack_push(size_t size, size_t align) {
    LFIContext* ctx = *get_thread_ctx();
    LFIRegs* regs = lfi_ctx_regs(ctx);
    uint64_t sp = detail::get_sp(regs);
    sp = (sp - size) & ~(align - 1);
    detail::set_sp(regs, sp);
    return reinterpret_cast<void*>(sp);
}

uint64_t Sandbox<LFI>::stack_save() {
    LFIContext* ctx = *get_thread_ctx();
    return detail::get_sp(lfi_ctx_regs(ctx));
}

void Sandbox<LFI>::stack_restore(uint64_t sp) {
    LFIContext* ctx = *get_thread_ctx();
    detail::set_sp(lfi_ctx_regs(ctx), sp);
}

void* Sandbox<LFI>::mmap(void* addr, size_t length, int prot, int flags,
                          int fd, off_t offset) {
    int lfi_prot = 0;
    if (prot & PROT_READ) lfi_prot |= LFI_PROT_READ;
    if (prot & PROT_WRITE) lfi_prot |= LFI_PROT_WRITE;
    if (prot & PROT_EXEC) lfi_prot |= LFI_PROT_EXEC;

    int lfi_flags = 0;
    if (flags & MAP_SHARED) lfi_flags |= LFI_MAP_SHARED;
    if (flags & MAP_PRIVATE) lfi_flags |= LFI_MAP_PRIVATE;
    if (flags & MAP_FIXED) lfi_flags |= LFI_MAP_FIXED;
    if (flags & MAP_ANONYMOUS) lfi_flags |= LFI_MAP_ANONYMOUS;

    lfiptr result;
    if (flags & MAP_FIXED) {
        result = lfi_box_mapat(box_, reinterpret_cast<lfiptr>(addr),
                               length, lfi_prot, lfi_flags, fd, offset);
    } else {
        result = lfi_box_mapany(box_, length, lfi_prot, lfi_flags, fd, offset);
    }
    return reinterpret_cast<void*>(result);
}

int Sandbox<LFI>::munmap(void* addr, size_t length) {
    return lfi_box_munmap(box_, reinterpret_cast<lfiptr>(addr), length);
}

void Sandbox<LFI>::idmem_reset() {
    std::lock_guard<std::mutex> lock(idmem_mutex_);
    for (void* p : idmem_allocations_) {
        lfi_lib_free(box_, get_thread_ctx(), p);
    }
    idmem_allocations_.clear();
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

}  // namespace sbox
