#include "KProcess.h"
#include <nce.h>
#include <os.h>
#include <fcntl.h>
#include <unistd.h>

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

    u64 KProcess::GetTlsSlot() {
        for (auto &tlsPage: tlsPages) {
            if (!tlsPage->Full())
                return tlsPage->ReserveSlot();
        }
        auto tlsMem = NewHandle<KPrivateMemory>(0, PAGE_SIZE, memory::Permission(true, true, false), memory::Type::ThreadLocal, threadMap.at(pid)).item;
        memoryMap[tlsMem->address] = tlsMem;
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->address));
        auto &tlsPage = tlsPages.back();
        if (tlsPages.empty())
            tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    KProcess::KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, u64 stackBase, u64 stackSize, std::shared_ptr<type::KSharedMemory> &tlsMemory) : pid(pid), mainThreadStackSz(stackSize), KSyncObject(state, KType::KProcess) {
        auto thread = NewHandle<KThread>(pid, entryPoint, 0x0, stackBase + stackSize, 0, constant::DefaultPriority, this, tlsMemory).item;
        // Remove GetTlsSlot from KThread ctor and cleanup ctor in general
        threadMap[pid] = thread;
        state.nce->WaitThreadInit(thread);
        thread->tls = GetTlsSlot();
        MapPrivateRegion(constant::HeapAddr, constant::DefHeapSize, {true, true, false}, memory::Type::Heap, memory::Region::Heap);
        memFd = open(fmt::format("/proc/{}/mem", pid).c_str(), O_RDWR | O_CLOEXEC);
        if (memFd == -1)
            throw exception("Cannot open file descriptor to /proc/{}/mem, \"{}\"", pid, strerror(errno));
    }

    KProcess::~KProcess() {
        close(memFd);
        status = Status::Exiting;
    }

    /**
     * @brief Function executed by all child threads after cloning
     */
    int ExecuteChild(void *) {
        asm volatile("BRK #0xFF"); // BRK #constant::brkRdy (So we know when the thread/process is ready)
        return 0;
    }

    u64 CreateThreadFunc(u64 stackTop) {
        pid_t pid = clone(&ExecuteChild, reinterpret_cast<void *>(stackTop), CLONE_THREAD | CLONE_SIGHAND | CLONE_PTRACE | CLONE_FS | CLONE_VM | CLONE_FILES | CLONE_IO, nullptr);
        return static_cast<u64>(pid);
    }

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, u8 priority) {
        /*
         * Future Reference:
         * https://android.googlesource.com/platform/bionic/+/master/libc/bionic/clone.cpp
         * https://android.googlesource.com/platform/bionic/+/master/libc/arch-arm64/bionic/__bionic_clone.S
        Registers fregs{};
        fregs.regs[0] = entryPoint;
        fregs.regs[1] = stackTop;
        fregs.x8 = __NR_clone;
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs, state.thread->pid);
        auto pid = static_cast<pid_t>(fregs.regs[0]);
        if (pid == -1)
            throw exception("Cannot create thread: Address: 0x{:X}, Stack Top: 0x{:X}", entryPoint, stackTop);
        auto process = NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(), priority, this).item;
        threadMap[pid] = process;
        return process;
        */
        return nullptr;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        pwrite64(memFd, source, size, offset);
    }

    KProcess::HandleOut<KPrivateMemory> KProcess::MapPrivateRegion(u64 address, size_t size, const memory::Permission perms, const memory::Type type, const memory::Region region) {
        auto mem = NewHandle<KPrivateMemory>(address, size, perms, type, threadMap.at(pid));
        memoryMap[mem.item->address] = mem.item;
        memoryRegionMap[region] = mem.item;
        return mem;
    }

    bool KProcess::UnmapPrivateRegion(const skyline::memory::Region region) {
        if (!memoryRegionMap.count(region))
            return false;
        memoryMap.erase(memoryRegionMap.at(region)->address);
        memoryRegionMap.erase(region);
        return true;
    }

    size_t KProcess::GetProgramSize() {
        size_t sharedSize = 0;
        for (auto &region : memoryRegionMap)
            sharedSize += region.second->size;
        return sharedSize;
    }

    void KProcess::MutexLock(u64 address) {
        try {
            auto mtx = mutexMap.at(address);
            pthread_mutex_lock(&mtx);
            u32 mtxVal = ReadMemory<u32>(address);
            mtxVal = (mtxVal & ~constant::MtxOwnerMask) | state.thread->handle;
            WriteMemory(mtxVal, address);
        } catch (const std::out_of_range &) {
            mutexMap[address] = PTHREAD_MUTEX_INITIALIZER;
        }
    }

    void KProcess::MutexUnlock(u64 address) {
        try {
            auto mtx = mutexMap.at(address);
            u32 mtxVal = ReadMemory<u32>(address);
            if ((mtxVal & constant::MtxOwnerMask) != state.thread->handle)
                throw exception("A non-owner thread tried to release a mutex");
            mtxVal = 0;
            WriteMemory(mtxVal, address);
            pthread_mutex_unlock(&mtx);
        } catch (const std::out_of_range &) {
            mutexMap[address] = PTHREAD_MUTEX_INITIALIZER;
        }
    }
}
