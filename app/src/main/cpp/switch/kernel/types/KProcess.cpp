#include "KProcess.h"
#include "../../nce.h"
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace lightSwitch::kernel::type {
    KProcess::tls_page_t::tls_page_t(uint64_t address) : address(address) {}

    uint64_t KProcess::tls_page_t::ReserveSlot() {
        if (Full())
            throw exception("Trying to get TLS slot from full page");
        slot[index] = true;
        return Get(index++); // ++ on right will cause increment after evaluation of expression
    }

    uint64_t KProcess::tls_page_t::Get(uint8_t slot_no) {
        if(slot_no>=constant::tls_slots)
            throw exception("TLS slot is out of range");
        return address + (constant::tls_slot_size * slot_no);
    }

    bool KProcess::tls_page_t::Full() {
        return slot[constant::tls_slots - 1];
    }

    uint64_t KProcess::GetTLSSlot(bool init) {
        if (!init)
            for (auto &tls_page: tls_pages) {
                if (!tls_page->Full())
                    return tls_page->ReserveSlot();
            }
        uint64_t address = MapPrivate(0, PAGE_SIZE, {true, true, false}, memory::Region::tls);
        tls_pages.push_back(std::make_shared<tls_page_t>(address));
        auto &tls_page = tls_pages.back();
        if (init)
            tls_page->ReserveSlot(); // User-mode exception handling
        return tls_page->ReserveSlot();
    }

    KProcess::KProcess(pid_t pid, uint64_t entry_point, uint64_t stack_base, uint64_t stack_size, const device_state &state, handle_t handle) : state(state), handle(handle), main_thread_stack_sz(stack_size), KObject(handle, KObjectType::KProcess) {
        process_state = process_state_t::Created;
        main_thread = pid;
        state.nce->WaitRdy(pid);
        thread_map[main_thread] = std::make_shared<KThread>(handle_index, pid, entry_point, 0, stack_base + stack_size, GetTLSSlot(true), constant::default_priority, this, state);
        NewHandle(std::static_pointer_cast<KObject>(thread_map[main_thread]));
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

    uint64_t CreateThreadFunc(uint64_t stack_top) {
        pid_t pid = clone(&ExecuteChild, reinterpret_cast<void *>(stack_top), CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM, nullptr); // NOLINT(hicpp-signed-bitwise)
        return static_cast<uint64_t>(pid);
    }

    std::shared_ptr<KThread> KProcess::CreateThread(uint64_t entry_point, uint64_t entry_arg, uint64_t stack_top, uint8_t priority) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = entry_point;
        fregs.regs[1] = stack_top;
        state.nce->ExecuteFunction((void *) CreateThreadFunc, fregs, main_thread);
        if (fregs.regs[0]==-1)
            throw exception(fmt::format("Cannot create thread: Address: {}, Stack Top: {}", entry_point, stack_top));
        auto thread = std::make_shared<kernel::type::KThread>(handle_index, static_cast<pid_t>(fregs.regs[0]), entry_point, entry_arg, stack_top, GetTLSSlot(false), priority, this, state);
        NewHandle(std::static_pointer_cast<KObject>(thread));
        return thread;
    }

    template<typename T>
    T KProcess::ReadMemory(uint64_t address) const {
        T item{};
        ReadMemory(&item, address, sizeof(T));
        return item;
    }

    template<typename T>
    void KProcess::WriteMemory(T &item, uint64_t address) const {
        WriteMemory(&item, address, sizeof(T));
    }

    void KProcess::ReadMemory(void *destination, uint64_t offset, size_t size) const {
        state.logger->Write(Logger::DEBUG, "ReadMemory for DE: 0x{:X}, OF: 0x{:X}, SZ: 0x{:X}", (uint64_t) destination, offset, size);
        pread64(mem_fd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, uint64_t offset, size_t size) const {
        state.logger->Write(Logger::DEBUG, "WriteMemory for SRC: 0x{:X}, OF: 0x{:X}, SZ: 0x{:X}", (uint64_t) source, offset, size);
        pwrite64(mem_fd, source, size, offset);
    }

    uint64_t MapPrivateFunc(uint64_t address, size_t size, uint64_t perms) {
        int flags = MAP_PRIVATE | MAP_ANONYMOUS; // NOLINT(hicpp-signed-bitwise)
        if (address) flags |= MAP_FIXED; // NOLINT(hicpp-signed-bitwise)
        return reinterpret_cast<uint64_t>(mmap(reinterpret_cast<void *>(address), size, static_cast<int>(perms), flags, -1, 0));
    }

    uint64_t KProcess::MapPrivate(uint64_t address, size_t size, const memory::Permission perms) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<uint64_t>(perms.get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(MapPrivateFunc), fregs, main_thread);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while mapping private region");
        state.logger->Write(Logger::DEBUG, "MapPrivate for ADR: 0x{:X}, SZ: 0x{:X}", fregs.regs[0], size);
        return fregs.regs[0];
    }

    uint64_t KProcess::MapPrivate(uint64_t address, size_t size, const memory::Permission perms, const memory::Region region) {
        uint64_t addr = MapPrivate(address,size, perms);
        memory_map.insert(std::pair<memory::Region, memory::RegionData>(region, {addr, size, perms}));
        return addr;
    }

    uint64_t RemapPrivateFunc(uint64_t address, size_t old_size, size_t size) {
        return (uint64_t) mremap(reinterpret_cast<void *>(address), old_size, size, 0);
    }

    void KProcess::RemapPrivate(uint64_t address, size_t old_size, size_t size) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = old_size;
        fregs.regs[2] = size;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(RemapPrivateFunc), fregs, main_thread);
        if (reinterpret_cast<void *>(fregs.regs[0]) == MAP_FAILED)
            throw exception("An error occurred while remapping private region");
    }

    void KProcess::RemapPrivate(const memory::Region region, size_t size) {
        memory::RegionData region_data = memory_map.at(region);
        RemapPrivate(region_data.address, region_data.size, size);
        region_data.size = size;
    }

    int UpdatePermissionPrivateFunc(uint64_t address, size_t size, uint64_t perms) {
        return mprotect(reinterpret_cast<void *>(address), size, static_cast<int>(perms));
    }

    void KProcess::UpdatePermissionPrivate(uint64_t address, size_t size, const memory::Permission perms) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        fregs.regs[2] = static_cast<uint64_t>(perms.get());
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UpdatePermissionPrivateFunc), fregs, main_thread);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while updating private region's permissions");
    }

    void KProcess::UpdatePermissionPrivate(const memory::Region region, const memory::Permission perms) {
        memory::RegionData region_data = memory_map.at(region);
        if (region_data.perms != perms) {
            UpdatePermissionPrivate(region_data.address, region_data.size, perms);
            region_data.perms = perms;
        }
    }

    uint64_t UnmapPrivateFunc(uint64_t address, size_t size) {
        return static_cast<uint64_t>(munmap(reinterpret_cast<void *>(address), size));
    }

    void KProcess::UnmapPrivate(uint64_t address, size_t size) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = address;
        fregs.regs[1] = size;
        state.nce->ExecuteFunction(reinterpret_cast<void *>(UnmapPrivateFunc), fregs, main_thread);
        if (static_cast<int>(fregs.regs[0]) == -1)
            throw exception("An error occurred while unmapping private region");
    }

    void KProcess::UnmapPrivate(const memory::Region region) {
        memory::RegionData region_data = memory_map.at(region);
        UnmapPrivate(region_data.address, region_data.size);
        memory_map.erase(region);
    }

    handle_t KProcess::NewHandle(std::shared_ptr<KObject> obj) {
        handle_table[handle_index] = std::move(obj);
        state.logger->Write(Logger::DEBUG, "Creating handle index 0x{0:X}", handle_index);
        return handle_index++; // Increment value after return
    }
}
