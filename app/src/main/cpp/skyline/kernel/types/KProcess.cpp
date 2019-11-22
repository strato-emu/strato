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
        auto tlsMem = NewHandle<KPrivateMemory>(mainThread, 0, 0, PAGE_SIZE, memory::Permission(true, true, false), memory::Type::ThreadLocal).item;
        memoryMap[tlsMem->address] = tlsMem;
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->address));
        auto &tlsPage = tlsPages.back();
        if (tlsPages.empty())
            tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    KProcess::KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, u64 stackBase, u64 stackSize) : mainThread(pid), mainThreadStackSz(stackSize), KSyncObject(state, KType::KProcess) {
        state.nce->WaitRdy(pid);
        threadMap[pid] = NewHandle<KThread>(pid, entryPoint, 0x0, stackBase + stackSize, GetTlsSlot(), constant::DefaultPriority, this).item;
        MapPrivateRegion(constant::HeapAddr, constant::DefHeapSize, {true, true, false}, memory::Type::Heap, memory::Region::Heap);
        memFd = open(fmt::format("/proc/{}/mem", pid).c_str(), O_RDWR | O_CLOEXEC); // NOLINT(hicpp-signed-bitwise)
        if (memFd == -1)
            throw exception("Cannot open file descriptor to /proc/{}/mem, \"{}\"", pid, strerror(errno));
    }

    KProcess::~KProcess() {
        close(memFd);
    }

    /**
     * @brief Function executed by all child threads after cloning
     */
    int ExecuteChild(void *) {
        asm volatile("BRK #0xFF"); // BRK #constant::brkRdy (So we know when the thread/process is ready)
        return 0;
    }

    u64 CreateThreadFunc(u64 stackTop) {
        pid_t pid = clone(&ExecuteChild, reinterpret_cast<void *>(stackTop), CLONE_THREAD | CLONE_SIGHAND | CLONE_PTRACE | CLONE_FS | CLONE_VM | CLONE_FILES | CLONE_IO, nullptr); // NOLINT(hicpp-signed-bitwise)
        return static_cast<u64>(pid);
    }

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, u8 priority) {
        user_pt_regs fregs = {0};
        fregs.regs[0] = entryPoint;
        fregs.regs[1] = stackTop;
        state.nce->ExecuteFunction((void *) CreateThreadFunc, fregs, mainThread);
        auto pid = static_cast<pid_t>(fregs.regs[0]);
        if (pid == -1)
            throw exception("Cannot create thread: Address: 0x{:X}, Stack Top: 0x{:X}", entryPoint, stackTop);
        auto process = NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(), priority, this).item;
        threadMap[pid] = process;
        state.os->processMap[pid] = state.os->processMap[mainThread];
        return process;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        pwrite64(memFd, source, size, offset);
    }

    int KProcess::GetMemoryFd() const {
        return memFd;
    }

    KProcess::HandleOut<KPrivateMemory> KProcess::MapPrivateRegion(u64 address, size_t size, const memory::Permission perms, const memory::Type type, const memory::Region region) {
        auto mem = NewHandle<KPrivateMemory>(mainThread, address, 0, size, perms, type);
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
        auto mtxVec = state.thisProcess->mutexMap[address];
        u32 mtxVal = state.thisProcess->ReadMemory<u32>(address);
        if (mtxVec.empty()) {
            mtxVal = (mtxVal & ~constant::MtxOwnerMask) | state.thisThread->handle;
            state.thisProcess->WriteMemory(mtxVal, address);
        } else {
            for (auto thread = mtxVec.begin();; thread++) {
                if ((*thread)->priority < state.thisThread->priority) {
                    mtxVec.insert(thread, state.thisThread);
                    break;
                } else if (thread + 1 == mtxVec.end()) {
                    mtxVec.push_back(state.thisThread);
                    break;
                }
            }
            state.thisThread->status = KThread::Status::WaitMutex;
        }
    }

    void KProcess::MutexUnlock(u64 address) {
        auto mtxVec = state.thisProcess->mutexMap[address];
        u32 mtxVal = state.thisProcess->ReadMemory<u32>(address);
        if ((mtxVal & constant::MtxOwnerMask) != state.thisThread->pid)
            throw exception("A non-owner thread tried to release a mutex");
        if (mtxVec.empty()) {
            mtxVal = 0;
        } else {
            auto &thread = mtxVec.front();
            mtxVal = (mtxVal & ~constant::MtxOwnerMask) | thread->handle;
            thread->status = KThread::Status::Runnable;
            mtxVec.erase(mtxVec.begin());
            if (!mtxVec.empty())
                mtxVal |= (~constant::MtxOwnerMask);
        }
        state.thisProcess->WriteMemory(mtxVal, address);
    }

    void KProcess::ResetSignal() {
        signalled = false;
    }
}
