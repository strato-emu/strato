// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <asm/unistd.h>
#include <nce/guest.h>
#include <nce.h>
#include <os.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KProcess::TlsPage::TlsPage(u8* ptr) : ptr(ptr) {}

    u8* KProcess::TlsPage::ReserveSlot() {
        if (Full())
            throw exception("Trying to get TLS slot from full page");

        slot[index] = true;
        return Get(index++); // ++ on right will cause increment after evaluation of expression
    }

    u8* KProcess::TlsPage::Get(u8 slotNo) {
        if (slotNo >= constant::TlsSlots)
            throw exception("TLS slot is out of range");

        return ptr + (constant::TlsSlotSize * slotNo);
    }

    bool KProcess::TlsPage::Full() {
        return slot[constant::TlsSlots - 1];
    }

    u8* KProcess::GetTlsSlot() {
        for (auto &tlsPage: tlsPages)
            if (!tlsPage->Full())
                return tlsPage->ReserveSlot();

        u8* ptr;
        if (tlsPages.empty()) {
            auto region{state.os->memory.tlsIo};
            ptr = reinterpret_cast<u8*>(region.size ? region.address : 0);
        } else {
            ptr = (*(tlsPages.end() - 1))->ptr + PAGE_SIZE;
        }

        auto tlsMem{NewHandle<KPrivateMemory>(ptr, PAGE_SIZE, memory::Permission(true, true, false), memory::states::ThreadLocal).item};
        tlsPages.push_back(std::make_shared<TlsPage>(tlsMem->ptr));

        auto &tlsPage{tlsPages.back()};
        if (tlsPages.empty())
            tlsPage->ReserveSlot(); // User-mode exception handling

        return tlsPage->ReserveSlot();
    }

    void KProcess::InitializeMemory() {
        constexpr size_t DefHeapSize{0x200000}; // The default amount of heap
        heap = NewHandle<KPrivateMemory>(reinterpret_cast<u8*>(state.os->memory.heap.address), DefHeapSize, memory::Permission{true, true, false}, memory::states::Heap).item;
        threads[pid]->tls = GetTlsSlot();
    }

    KProcess::KProcess(const DeviceState &state, pid_t pid, u64 entryPoint, std::shared_ptr<type::KSharedMemory> &stack, std::shared_ptr<type::KSharedMemory> &tlsMemory) : pid(pid), stack(stack), KSyncObject(state, KType::KProcess) {
        constexpr u8 DefaultPriority{44}; // The default priority of a process

        auto thread{NewHandle<KThread>(pid, entryPoint, 0, reinterpret_cast<u64>(stack->guest.ptr + stack->guest.size), nullptr, DefaultPriority, this, tlsMemory).item};
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

    std::shared_ptr<KThread> KProcess::CreateThread(u64 entryPoint, u64 entryArg, u64 stackTop, i8 priority) {
        auto size{(sizeof(ThreadContext) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1)};
        auto tlsMem{std::make_shared<type::KSharedMemory>(state, size, memory::states::Reserved)};

        Registers fregs{
            .x0 = CLONE_THREAD | CLONE_SIGHAND | CLONE_PTRACE | CLONE_FS | CLONE_VM | CLONE_FILES | CLONE_IO,
            .x1 = stackTop,
            .x3 = reinterpret_cast<u64>(tlsMem->Map(nullptr, size, memory::Permission{true, true, false})),
            .x8 = __NR_clone,
            .x5 = reinterpret_cast<u64>(&guest::GuestEntry),
            .x6 = entryPoint,
        };

        state.nce->ExecuteFunction(ThreadCall::Clone, fregs);
        if (static_cast<int>(fregs.x0) < 0)
            throw exception("Cannot create thread: Address: 0x{:X}, Stack Top: 0x{:X}", entryPoint, stackTop);

        auto pid{static_cast<pid_t>(fregs.x0)};
        auto thread{NewHandle<KThread>(pid, entryPoint, entryArg, stackTop, GetTlsSlot(), priority, this, tlsMem).item};
        threads[pid] = thread;

        return thread;
    }

    void KProcess::ReadMemory(void *destination, u64 offset, size_t size, bool forceGuest) {
        if (!forceGuest) {
            auto source{reinterpret_cast<u8*>(offset)};

            if (source) {
                std::memcpy(destination, reinterpret_cast<void *>(source), size);
                return;
            }
        }

        struct iovec local{
            .iov_base = destination,
            .iov_len = size,
        };

        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size,
        };

        if (process_vm_readv(pid, &local, 1, &remote, 1, 0) < 0)
            pread64(memFd, destination, size, offset);
    }

    void KProcess::WriteMemory(const void *source, u64 offset, size_t size, bool forceGuest) {
        if (!forceGuest) {
            auto destination{reinterpret_cast<u8*>(offset)};

            if (destination) {
                std::memcpy(reinterpret_cast<void *>(destination), source, size);
                return;
            }
        }

        struct iovec local{
            .iov_base = const_cast<void *>(source),
            .iov_len = size,
        };

        struct iovec remote{
            .iov_base = reinterpret_cast<void *>(offset),
            .iov_len = size,
        };

        if (process_vm_writev(pid, &local, 1, &remote, 1, 0) < 0)
            pwrite64(memFd, source, size, offset);
    }

    std::optional<KProcess::HandleOut<KMemory>> KProcess::GetMemoryObject(u8* ptr) {
        for (KHandle index{}; index < handles.size(); index++) {
            auto& object{handles[index]};
            switch (object->objectType) {
                case type::KType::KPrivateMemory:
                case type::KType::KSharedMemory:
                case type::KType::KTransferMemory: {
                    auto mem{std::static_pointer_cast<type::KMemory>(object)};
                    if (mem->IsInside(ptr))
                        return std::make_optional<KProcess::HandleOut<KMemory>>({mem, constant::BaseHandleIndex + index});
                }

                default:
                    break;
            }
        }
        return std::nullopt;
    }

    bool KProcess::MutexLock(u64 address, KHandle owner) {
        std::unique_lock lock(mutexLock);

        auto mtx{GetPointer<u32>(address)};
        auto &mtxWaiters{mutexes[address]};

        if (mtxWaiters.empty()) {
            u32 mtxExpected{};
            if (__atomic_compare_exchange_n(mtx, &mtxExpected, (constant::MtxOwnerMask & state.thread->handle), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                return true;
        }

        if (__atomic_load_n(mtx, __ATOMIC_SEQ_CST) != (owner | ~constant::MtxOwnerMask))
            return false;

        std::shared_ptr<WaitStatus> status;
        for (auto it{mtxWaiters.begin()};; it++) {
            if (it != mtxWaiters.end() && (*it)->priority >= state.thread->priority)
                continue;

            status = std::make_shared<WaitStatus>(state.thread->priority, state.thread->handle);
            mtxWaiters.insert(it, status);
            break;
        }

        lock.unlock();
        while (!status->flag);
        lock.lock();
        status->flag = false;

        for (auto it{mtxWaiters.begin()}; it != mtxWaiters.end(); it++) {
            if ((*it)->handle == state.thread->handle) {
                mtxWaiters.erase(it);
                break;
            }
        }

        return true;
    }

    bool KProcess::MutexUnlock(u64 address) {
        std::unique_lock lock(mutexLock);

        auto mtx{GetPointer<u32>(address)};
        auto &mtxWaiters{mutexes[address]};
        u32 mtxDesired{};
        if (!mtxWaiters.empty())
            mtxDesired = (*mtxWaiters.begin())->handle | ((mtxWaiters.size() > 1) ? ~constant::MtxOwnerMask : 0);

        u32 mtxExpected{(constant::MtxOwnerMask & state.thread->handle) | ~constant::MtxOwnerMask};
        if (!__atomic_compare_exchange_n(mtx, &mtxExpected, mtxDesired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            mtxExpected &= constant::MtxOwnerMask;

            if (!__atomic_compare_exchange_n(mtx, &mtxExpected, mtxDesired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                return false;
        }

        if (mtxDesired) {
            auto status{(*mtxWaiters.begin())};
            status->flag = true;
            lock.unlock();
            while (status->flag);
            lock.lock();
        }

        return true;
    }

    bool KProcess::ConditionalVariableWait(u64 conditionalAddress, u64 mutexAddress, u64 timeout) {
        std::unique_lock lock(conditionalLock);
        auto &condWaiters{conditionals[conditionalAddress]};

        std::shared_ptr<WaitStatus> status;
        for (auto it{condWaiters.begin()};; it++) {
            if (it != condWaiters.end() && (*it)->priority >= state.thread->priority)
                continue;

            status = std::make_shared<WaitStatus>(state.thread->priority, state.thread->handle, mutexAddress);
            condWaiters.insert(it, status);
            break;
        }

        lock.unlock();

        bool timedOut{};
        auto start{util::GetTimeNs()};
        while (!status->flag)
            if ((util::GetTimeNs() - start) >= timeout)
                timedOut = true;

        lock.lock();

        if (!status->flag)
            timedOut = false;
        else
            status->flag = false;

        for (auto it{condWaiters.begin()}; it != condWaiters.end(); it++) {
            if ((*it)->handle == state.thread->handle) {
                condWaiters.erase(it);
                break;
            }
        }

        lock.unlock();

        return !timedOut;
    }

    void KProcess::ConditionalVariableSignal(u64 address, u64 amount) {
        std::unique_lock condLock(conditionalLock);

        auto &condWaiters{conditionals[address]};
        u64 count{};

        auto iter{condWaiters.begin()};
        while (iter != condWaiters.end() && count < amount) {
            auto &thread{*iter};
            auto mtx{GetPointer<u32>(thread->mutexAddress)};
            u32 mtxValue{__atomic_load_n(mtx, __ATOMIC_SEQ_CST)};

            while (true) {
                u32 mtxDesired{};

                if (!mtxValue)
                    mtxDesired = (constant::MtxOwnerMask & thread->handle);
                else if ((mtxValue & constant::MtxOwnerMask) == state.thread->handle)
                    mtxDesired = mtxValue | (constant::MtxOwnerMask & thread->handle);
                else if (mtxValue & ~constant::MtxOwnerMask)
                    mtxDesired = mtxValue | ~constant::MtxOwnerMask;
                else
                    break;

                if (__atomic_compare_exchange_n(mtx, &mtxValue, mtxDesired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                    break;
            }
            if (mtxValue && ((mtxValue & constant::MtxOwnerMask) != state.thread->handle)) {
                std::unique_lock mtxLock(mutexLock);

                auto &mtxWaiters{mutexes[thread->mutexAddress]};
                std::shared_ptr<WaitStatus> status;

                for (auto it{mtxWaiters.begin()};; it++) {
                    if (it != mtxWaiters.end() && (*it)->priority >= thread->priority)
                        continue;
                    status = std::make_shared<WaitStatus>(thread->priority, thread->handle);
                    mtxWaiters.insert(it, status);
                    break;
                }

                mtxLock.unlock();
                while (!status->flag);
                mtxLock.lock();
                status->flag = false;

                for (auto it{mtxWaiters.begin()}; it != mtxWaiters.end(); it++) {
                    if ((*it)->handle == thread->handle) {
                        mtxWaiters.erase(it);
                        break;
                    }
                }

                mtxLock.unlock();
            }

            thread->flag = true;
            iter++;
            count++;

            condLock.unlock();
            while (thread->flag);
            condLock.lock();
        }
    }
}
