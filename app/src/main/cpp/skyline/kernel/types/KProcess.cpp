#include "KProcess.h"
#include <nce.h>
#include <os.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>

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
        for (auto &tlsPage: tlsPages)
            if (!tlsPage->Full())
                return tlsPage->ReserveSlot();
        u64 address;
        if(tlsPages.empty())
            address = state.os->memory.GetRegion(memory::Regions::TlsIo).address;
        else
            address = (*(tlsPages.end()-1))->address + PAGE_SIZE;
        auto tlsMem = NewHandle<KPrivateMemory>(address, PAGE_SIZE, memory::Permission(true, true, false), memory::MemoryStates::ThreadLocal).item;
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->address));
        auto &tlsPage = tlsPages.back();
        if (tlsPages.empty())
            tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    void KProcess::InitializeMemory() {
        heap = NewHandle<KPrivateMemory>(state.os->memory.GetRegion(memory::Regions::Heap).address, constant::DefHeapSize, memory::Permission{true, true, false}, memory::MemoryStates::Heap).item;
        threads[pid]->tls = GetTlsSlot();
    }

    KProcess::KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, u64 stackBase, u64 stackSize, std::shared_ptr<type::KSharedMemory> &tlsMemory) : pid(pid), KSyncObject(state, KType::KProcess) {
        auto thread = NewHandle<KThread>(pid, entryPoint, 0x0, stackBase + stackSize, 0, constant::DefaultPriority, this, tlsMemory).item;
        threads[pid] = thread;
        state.nce->WaitThreadInit(thread);
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
        state.nce->ExecuteFunction(ThreadCall::Syscall, fregs);
        auto pid = static_cast<pid_t>(fregs.regs[0]);
        if (pid == -1)
            throw exception("Cannot create thread: Address: 0x{:X}, Stack Top: 0x{:X}", entryPoint, stackTop);
        auto process = NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(), priority, this).item;
        threads[pid] = process;
        return process;
        */
        return nullptr;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size) const {
        struct iovec local{
            .iov_base = destination,
            .iov_len = size
        };
        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size
        };

        if (process_vm_readv(pid, &local, 1, &remote, 1, 0) < 0)
            pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(void *source, u64 offset, size_t size) const {
        struct iovec local{
            .iov_base = source,
            .iov_len = size
        };
        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size
        };

        if (process_vm_writev(pid, &local, 1, &remote, 1, 0) < 0)
            pwrite64(memFd, source, size, offset);
    }

    void KProcess::CopyMemory(u64 source, u64 destination, size_t size) const {
        if (size <= PAGE_SIZE) {
            std::vector<u8> buffer(size);
            state.process->ReadMemory(buffer.data(), source, size);
            state.process->WriteMemory(buffer.data(), destination, size);
        } else {
            Registers fregs{};
            fregs.x0 = source;
            fregs.x1 = destination;
            fregs.x2 = size;
            state.nce->ExecuteFunction(ThreadCall::Memcopy, fregs);
        }
    }

    std::shared_ptr<KMemory> KProcess::GetMemoryObject(u64 address) {
        for(auto& [handle, object] : state.process->handles) {
            switch(object->objectType) {
                case type::KType::KPrivateMemory:
                case type::KType::KSharedMemory:
                case type::KType::KTransferMemory: {
                    auto mem = std::static_pointer_cast<type::KMemory>(object);
                    if (mem->IsInside(address))
                        return mem;
                }
                default:
                    break;
            }
        }
        return nullptr;
    }

    void KProcess::MutexLock(u64 address) {
        try {
            auto mtx = mutexes.at(address);
            pthread_mutex_lock(&mtx);
            u32 mtxVal = ReadMemory<u32>(address);
            mtxVal = (mtxVal & ~constant::MtxOwnerMask) | state.thread->handle;
            WriteMemory(mtxVal, address);
        } catch (const std::out_of_range &) {
            mutexes[address] = PTHREAD_MUTEX_INITIALIZER;
        }
    }

    void KProcess::MutexUnlock(u64 address) {
        try {
            auto mtx = mutexes.at(address);
            u32 mtxVal = ReadMemory<u32>(address);
            if ((mtxVal & constant::MtxOwnerMask) != state.thread->handle)
                throw exception("A non-owner thread tried to release a mutex");
            mtxVal = 0;
            WriteMemory(mtxVal, address);
            pthread_mutex_unlock(&mtx);
        } catch (const std::out_of_range &) {
            mutexes[address] = PTHREAD_MUTEX_INITIALIZER;
        }
    }
}
