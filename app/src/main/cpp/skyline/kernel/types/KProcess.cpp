#include "KProcess.h"
#include "../../nce.h"
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(u64 address) : address(address) {}

    u64 KProcess::TlsPage::ReserveSlot() {
        if (Full())
            throw exception("Trying to get TLS slot from full page");
        slot[index] = true;
        return Get(index++); // ++ on right will cause increment after evaluation of expression
    }

    u64 KProcess::TlsPage::Get(u8 slotNo) {
        if (slotNo >= constant::TlsSlots)
            throw exception("TLS slot is out of range");
        return address + (constant::TlsSlotSize * slotNo);
    }

    bool KProcess::TlsPage::Full() {
        return slot[constant::TlsSlots - 1];
    }

    u64 KProcess::GetTlsSlot(bool init) {
        if (!init)
            for (auto &tlsPage: tlsPages) {
                if (!tlsPage->Full())
                    return tlsPage->ReserveSlot();
            }
        auto tlsMem = NewHandle<KPrivateMemory>(0, 0, PAGE_SIZE, memory::Permission(true, true, false), memory::Type::ThreadLocal);
        memoryMap[tlsMem->address] = tlsMem;
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->address));
        auto &tlsPage = tlsPages.back();
        if (init)
            tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    KProcess::KProcess(handle_t handle, pid_t pid, const DeviceState &state, u64 entryPoint, u64 stackBase, u64 stackSize) : mainThread(pid), mainThreadStackSz(stackSize), KSyncObject(handle, pid, state, KType::KProcess) {
        state.nce->WaitRdy(pid);
        threadMap[mainThread] = NewHandle<KThread>(pid, entryPoint, 0, stackBase + stackSize, GetTlsSlot(true), constant::DefaultPriority, this);
        MapPrivateRegion(0, constant::DefHeapSize, {true, true, true}, memory::Type::Heap, memory::Region::Heap);
        for (auto &region : state.nce->memoryRegionMap)
            region.second->InitiateProcess(pid);
        memFd = open(fmt::format("/proc/{}/mem", pid).c_str(), O_RDWR | O_CLOEXEC); // NOLINT(hicpp-signed-bitwise)
        if (memFd == -1)
            throw exception(fmt::format("Cannot open file descriptor to /proc/{}/mem", pid));
    }

    KProcess::~KProcess() {
        close(memFd);
    }

    /**
     * Function executed by all child threads after cloning
     */
    int ExecuteChild(void *) {
        ptrace(PTRACE_TRACEME);
        asm volatile("BRK #0xFF"); // BRK #constant::brkRdy (So we know when the thread/process is ready)
        return 0;
    }

    u64 CreateThreadFunc(u64 stackTop) {
        pid_t pid = clone(&ExecuteChild, reinterpret_cast<void *>(stackTop), CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM, nullptr); // NOLINT(hicpp-signed-bitwise)
        return static_cast<u64>(pid);
    }

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, u8 priority) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = entryPoint;
        fregs.regs[1] = stackTop;
        state.nce->ExecuteFunction((void *) CreateThreadFunc, fregs, mainThread);
        auto pid = static_cast<pid_t>(fregs.regs[0]);
        if (pid == -1)
            throw exception(fmt::format("Cannot create thread: Address: {}, Stack Top: {}", entryPoint, stackTop));
        threadMap[pid] = NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(false), priority, this);
        return threadMap[pid];
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        pwrite64(memFd, source, size, offset);
    }

    std::shared_ptr<KPrivateMemory> KProcess::MapPrivateRegion(u64 address, size_t size, const memory::Permission perms, const memory::Type type, const memory::Region region) {
        auto item = NewHandle<KPrivateMemory>(address, 0, size, perms, type);
        memoryMap[item->address] = item;
        memoryRegionMap[region] = item;
        return item;
    }
}
