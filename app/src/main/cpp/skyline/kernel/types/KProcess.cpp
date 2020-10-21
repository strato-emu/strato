// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include "KProcess.h"

namespace skyline::kernel::type {
    KProcess::WaitStatus::WaitStatus(i8 priority, KHandle handle) : priority(priority), handle(handle) {}

    KProcess::WaitStatus::WaitStatus(i8 priority, KHandle handle, u32 *mutex) : priority(priority), handle(handle), mutex(mutex) {}

    KProcess::TlsPage::TlsPage(const std::shared_ptr<KPrivateMemory> &memory) : memory(memory) {}

    u8 *KProcess::TlsPage::ReserveSlot() {
        if (Full())
            throw exception("Trying to reserve TLS slot in full page");
        return Get(index++);
    }

    u8 *KProcess::TlsPage::Get(u8 index) {
        if (index >= constant::TlsSlots)
            throw exception("TLS slot is out of range");
        return memory->ptr + (constant::TlsSlotSize * index);
    }

    bool KProcess::TlsPage::Full() {
        return index == constant::TlsSlots;
    }

    KProcess::KProcess(const DeviceState &state) : memory(state), KSyncObject(state, KType::KProcess) {}

    void KProcess::InitializeHeap() {
        constexpr size_t DefaultHeapSize{0x200000};
        heap = heap.make_shared(state, reinterpret_cast<u8 *>(state.process->memory.heap.address), DefaultHeapSize, memory::Permission{true, true, false}, memory::states::Heap);
        InsertItem(heap); // Insert it into the handle table so GetMemoryObject will contain it
    }

    u8 *KProcess::AllocateTlsSlot() {
        for (auto &tlsPage: tlsPages)
            if (!tlsPage->Full())
                return tlsPage->ReserveSlot();

        u8 *ptr = tlsPages.empty() ? reinterpret_cast<u8 *>(state.process->memory.tlsIo.address) : ((*(tlsPages.end() - 1))->memory->ptr + PAGE_SIZE);
        auto tlsPage{std::make_shared<TlsPage>(std::make_shared<KPrivateMemory>(state, ptr, PAGE_SIZE, memory::Permission(true, true, false), memory::states::ThreadLocal))};
        tlsPages.push_back(tlsPage);

        tlsPage->ReserveSlot(); // User-mode exception handling
        return tlsPage->ReserveSlot();
    }

    std::shared_ptr<KThread> KProcess::CreateThread(void *entry, u64 argument, void *stackTop, i8 priority, i8 idealCore) {
        if (!stackTop && threads.empty()) { //!< Main thread stack is created by the kernel and owned by the process
            mainThreadStack = mainThreadStack.make_shared(state, reinterpret_cast<u8 *>(state.process->memory.stack.address), state.process->npdm.meta.mainThreadStackSize, memory::Permission{true, true, false}, memory::states::Stack);
            if (mprotect(mainThreadStack->ptr, PAGE_SIZE, PROT_NONE))
                throw exception("Failed to create guard page for thread stack at 0x{:X}", mainThreadStack->ptr);
            stackTop = mainThreadStack->ptr + mainThreadStack->size;
        }
        auto thread{NewHandle<KThread>(this, threads.size(), entry, argument, stackTop, (priority == -1) ? state.process->npdm.meta.mainThreadPriority : priority, (idealCore == -1) ? state.process->npdm.meta.idealCore : idealCore).item};
        threads.push_back(thread);
        return thread;
    }

    std::optional<KProcess::HandleOut<KMemory>> KProcess::GetMemoryObject(u8 *ptr) {
        std::shared_lock lock(handleMutex);

        for (KHandle index{}; index < handles.size(); index++) {
            auto &object{handles[index]};
            if (object) {
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
        }
        return std::nullopt;
    }

    bool KProcess::MutexLock(u32 *mutex, KHandle owner) {
        std::unique_lock lock(mutexLock);

        auto &mtxWaiters{mutexes[reinterpret_cast<u64>(mutex)]};
        if (mtxWaiters.empty()) {
            u32 mtxExpected{};
            if (__atomic_compare_exchange_n(mutex, &mtxExpected, (constant::MtxOwnerMask & state.thread->handle), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                return true;
        }

        if (__atomic_load_n(mutex, __ATOMIC_SEQ_CST) != (owner | ~constant::MtxOwnerMask))
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

    bool KProcess::MutexUnlock(u32 *mutex) {
        std::unique_lock lock(mutexLock);

        auto &mtxWaiters{mutexes[reinterpret_cast<u64>(mutex)]};
        u32 mtxDesired{};
        if (!mtxWaiters.empty())
            mtxDesired = (*mtxWaiters.begin())->handle | ((mtxWaiters.size() > 1) ? ~constant::MtxOwnerMask : 0);

        u32 mtxExpected{(constant::MtxOwnerMask & state.thread->handle) | ~constant::MtxOwnerMask};
        if (!__atomic_compare_exchange_n(mutex, &mtxExpected, mtxDesired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            mtxExpected &= constant::MtxOwnerMask;

            if (!__atomic_compare_exchange_n(mutex, &mtxExpected, mtxDesired, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
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

    bool KProcess::ConditionalVariableWait(void *conditional, u32 *mutex, u64 timeout) {
        std::unique_lock lock(conditionalLock);

        auto &condWaiters{conditionals[reinterpret_cast<u64>(conditional)]};
        std::shared_ptr<WaitStatus> status;
        for (auto it{condWaiters.begin()};; it++) {
            if (it != condWaiters.end() && (*it)->priority >= state.thread->priority)
                continue;

            status = std::make_shared<WaitStatus>(state.thread->priority, state.thread->handle, mutex);
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

    void KProcess::ConditionalVariableSignal(void *conditional, u64 amount) {
        std::unique_lock condLock(conditionalLock);

        auto &condWaiters{conditionals[reinterpret_cast<u64>(conditional)]};
        u64 count{};

        auto iter{condWaiters.begin()};
        while (iter != condWaiters.end() && count < amount) {
            auto &thread{*iter};
            auto mtx{thread->mutex};
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

                auto &mtxWaiters{mutexes[reinterpret_cast<u64>(thread->mutex)]};
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
