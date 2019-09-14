#include "KProcess.h"
#include "../../nce.h"
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace lightSwitch::kernel::type {
    KProcess::tls_page_t::tls_page_t(u64 address) : address(address) {}

    u64 KProcess::tls_page_t::ReserveSlot() {
        if (Full())
            throw exception("Trying to get TLS slot from full page");
        slot[index] = true;
        return Get(index++); // ++ on right will cause increment after evaluation of expression
    }

    u64 KProcess::tls_page_t::Get(u8 slot_no) {
        if (slot_no >= constant::tls_slots)
            throw exception("TLS slot is out of range");
        return address + (constant::tls_slot_size * slot_no);
    }

    bool KProcess::tls_page_t::Full() {
        return slot[constant::tls_slots - 1];
    }

    u64 KProcess::GetTLSSlot(bool init) {
        if (!init)
            for (auto &tls_page: tls_pages) {
                if (!tls_page->Full())
                    return tls_page->ReserveSlot();
            }
        auto tls_mem = std::make_shared<KPrivateMemory>(KPrivateMemory(state, 0, 0, PAGE_SIZE, {true, true, false}, Memory::Type::ThreadLocal, main_thread));
        memory_map[tls_mem->address] = tls_mem;
        tls_pages.push_back(std::make_shared<tls_page_t>(tls_mem->address));
        auto &tls_page = tls_pages.back();
        if (init)
            tls_page->ReserveSlot(); // User-mode exception handling
        return tls_page->ReserveSlot();
    }

    KProcess::KProcess(pid_t pid, u64 entry_point, u64 stack_base, u64 stack_size, const device_state &state, handle_t handle) : state(state), handle(handle), main_thread_stack_sz(stack_size), KObject(handle, KObjectType::KProcess) {
        process_state = process_state_t::Created;
        main_thread = pid;
        state.nce->WaitRdy(pid);
        thread_map[main_thread] = std::make_shared<KThread>(handle_index, pid, entry_point, 0, stack_base + stack_size, GetTLSSlot(true), constant::default_priority, this, state);
        NewHandle(std::static_pointer_cast<KObject>(thread_map[main_thread]));
        MapPrivateRegion(0, constant::def_heap_size, {true, true, true}, Memory::Type::Heap, Memory::Region::heap);
        for (auto &region : state.nce->memory_map) {
            region.second->InitiateProcess(pid);
        }
        mem_fd = open(fmt::format("/proc/{}/mem", pid).c_str(), O_RDWR | O_CLOEXEC); // NOLINT(hicpp-signed-bitwise)
        if (mem_fd == -1) throw exception(fmt::format("Cannot open file descriptor to /proc/{}/mem", pid));
    }

    KProcess::~KProcess() {
        close(mem_fd);
    }

    /**
     * Function executed by all child threads after cloning
     */
    int ExecuteChild(void *) {
        ptrace(PTRACE_TRACEME);
        asm volatile("brk #0xFF"); // BRK #constant::brk_rdy (So we know when the thread/process is ready)
        return 0;
    }

    u64 CreateThreadFunc(u64 stack_top) {
        pid_t pid = clone(&ExecuteChild, reinterpret_cast<void *>(stack_top), CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM, nullptr); // NOLINT(hicpp-signed-bitwise)
        return static_cast<u64>(pid);
    }

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entry_point, u64 entry_arg, u64 stack_top, u8 priority) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = entry_point;
        fregs.regs[1] = stack_top;
        state.nce->ExecuteFunction((void *) CreateThreadFunc, fregs, main_thread);
        if (fregs.regs[0] == -1)
            throw exception(fmt::format("Cannot create thread: Address: {}, Stack Top: {}", entry_point, stack_top));
        auto thread = std::make_shared<kernel::type::KThread>(handle_index, static_cast<pid_t>(fregs.regs[0]), entry_point, entry_arg, stack_top, GetTLSSlot(false), priority, this, state);
        NewHandle(std::static_pointer_cast<KObject>(thread));
        return thread;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        pread64(mem_fd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        pwrite64(mem_fd, source, size, offset);
    }

    std::shared_ptr<KPrivateMemory> KProcess::MapPrivateRegion(u64 address, size_t size, const Memory::Permission perms, const Memory::Type type, const Memory::Region region) {
        auto item = std::make_shared<KPrivateMemory>(state, address, 0, size, perms, type, main_thread);
        memory_map[item->address] = item;
        memory_region_map[region] = item;
        return item;
    }

    handle_t KProcess::NewHandle(std::shared_ptr<KObject> obj) {
        handle_table[handle_index] = std::move(obj);
        state.logger->Write(Logger::DEBUG, "Creating handle index 0x{0:X}", handle_index);
        return handle_index++; // Increment value after return
    }
}
