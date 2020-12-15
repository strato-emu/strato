// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <nce.h>
#include <os.h>
#include <kernel/results.h>
#include "KProcess.h"

namespace skyline::kernel::type {
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

    KProcess::~KProcess() {
        std::lock_guard guard(threadMutex);
        disableThreadCreation = true;
        for (const auto &thread : threads)
            thread->Kill(true);
    }

    void KProcess::Kill(bool join, bool all, bool disableCreation) {
        std::lock_guard guard(threadMutex);
        if (disableCreation)
            disableThreadCreation = true;
        if (all) {
            for (const auto &thread : threads)
                thread->Kill(join);
        } else {
            std::shared_ptr<KThread> thread;
            try {
                thread = threads.at(0);
            } catch (const std::out_of_range &) {
                return;
            }
            thread->Kill(join);
        }
    }

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

    std::shared_ptr<KThread> KProcess::CreateThread(void *entry, u64 argument, void *stackTop, std::optional<u8> priority, std::optional<u8> idealCore) {
        std::lock_guard guard(threadMutex);
        if (disableThreadCreation)
            return nullptr;
        if (!stackTop && threads.empty()) { //!< Main thread stack is created by the kernel and owned by the process
            mainThreadStack = mainThreadStack.make_shared(state, reinterpret_cast<u8 *>(state.process->memory.stack.address), state.process->npdm.meta.mainThreadStackSize, memory::Permission{true, true, false}, memory::states::Stack);
            if (mprotect(mainThreadStack->ptr, PAGE_SIZE, PROT_NONE))
                throw exception("Failed to create guard page for thread stack at 0x{:X}", mainThreadStack->ptr);
            stackTop = mainThreadStack->ptr + mainThreadStack->size;
        }
        auto thread{NewHandle<KThread>(this, threads.size(), entry, argument, stackTop, priority ? *priority : state.process->npdm.meta.mainThreadPriority, idealCore ? *idealCore : state.process->npdm.meta.idealCore).item};
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

    constexpr u32 HandleWaitMask{0x40000000}; //!< A mask of a bit which denotes if the handle has waiters or not


    Result KProcess::MutexLock(u32 *mutex, KHandle ownerHandle) {
        std::shared_ptr<KThread> owner;
        try {
            owner = GetHandle<KThread>(ownerHandle & ~HandleWaitMask);
        } catch (const std::out_of_range &) {
            return result::InvalidHandle;
        }

        bool isHighestPriority;
        {
            std::lock_guard lock(owner->waiterMutex);

            u32 value{};
            if (__atomic_compare_exchange_n(mutex, &value, (HandleWaitMask & state.thread->handle), false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
                // We try to do a CAS to get ownership of the mutex in the case that it's unoccupied
                return {};
            if (value != (ownerHandle | HandleWaitMask))
                // We ensure that the mutex's value is the handle with the waiter bit set
                return result::InvalidCurrentMemory;

            auto &waiters{owner->waiters};
            isHighestPriority = waiters.insert(std::upper_bound(waiters.begin(), waiters.end(), state.thread->priority.load(), KThread::IsHigherPriority), state.thread) == waiters.begin();
            state.scheduler->RemoveThread();

            std::atomic_store(&state.thread->waitThread, owner);
            state.thread->waitKey = mutex;
        }

        if (isHighestPriority) {
            // If we were the highest priority thread then we need to inherit priorities for all threads we're waiting on recursively
            do {
                u8 priority, ownerPriority;
                do {
                    // Try to CAS the priority of the owner with the current thread
                    // If they're equivalent then we don't need to CAS as the priority won't be inherited
                    ownerPriority = owner->priority.load();
                    priority = std::min(ownerPriority, state.thread->priority.load());
                } while (ownerPriority != priority && owner->priority.compare_exchange_strong(ownerPriority, priority));

                if (ownerPriority != priority) {
                    auto waitThread{std::atomic_load(&owner->waitThread)};
                    while (waitThread) {
                        std::lock_guard lock(waitThread->waiterMutex);

                        auto currentWaitThread{std::atomic_load(&owner->waitThread)};
                        if (waitThread != currentWaitThread) {
                            waitThread = currentWaitThread;
                            continue;
                        }

                        // We need to update the location of the owner thread in the waiter queue of the thread it's waiting on
                        auto &waiters{waitThread->waiters};
                        waiters.erase(std::find(waiters.begin(), waiters.end(), waitThread));
                        waiters.insert(std::upper_bound(waiters.begin(), waiters.end(), state.thread->priority.load(), KThread::IsHigherPriority), owner);
                        break;
                    }

                    state.scheduler->UpdatePriority(owner);

                    owner = waitThread;
                } else {
                    break;
                }
            } while (owner);
        }

        state.scheduler->WaitSchedule();

        return {};
    }

    void KProcess::MutexUnlock(u32 *mutex) {
        std::lock_guard lock(state.thread->waiterMutex);
        auto &waiters{state.thread->waiters};
        auto nextOwnerIt{std::find_if(waiters.begin(), waiters.end(), [mutex](const std::shared_ptr<KThread> &thread) { return thread->waitKey == mutex; })};
        if (nextOwnerIt != waiters.end()) {
            auto nextOwner{*nextOwnerIt};
            std::lock_guard nextLock(nextOwner->waiterMutex);
            std::atomic_store(&nextOwner->waitThread, std::shared_ptr<KThread>{nullptr});
            nextOwner->waitKey = nullptr;

            // Move all threads waiting on this key to the next owner's waiter list
            std::shared_ptr<KThread> nextWaiter{};
            for (auto it{waiters.erase(nextOwnerIt)}; it != waiters.end(); it++) {
                if ((*it)->waitKey == mutex) {
                    nextOwner->waiters.splice(std::upper_bound(nextOwner->waiters.begin(), nextOwner->waiters.end(), (*it)->priority.load(), KThread::IsHigherPriority), waiters, it);
                    std::atomic_store(&(*it)->waitThread, nextOwner);
                    if (!nextWaiter)
                        nextWaiter = *it;
                }
            }

            if (!waiters.empty()) {
                // If there are threads still waiting on us then try to inherit their priority
                auto highestPriority{waiters.front()};
                u8 priority, ownerPriority;
                do {
                    ownerPriority = state.thread->priority.load();
                    priority = std::min(ownerPriority, highestPriority->priority.load());
                } while (ownerPriority != priority && nextOwner->priority.compare_exchange_strong(ownerPriority, priority));
                state.scheduler->UpdatePriority(state.thread);
            }

            if (nextWaiter) {
                // If there is a waiter on the new owner then try to inherit it's priority
                u8 priority, ownerPriority;
                do {
                    ownerPriority = nextOwner->priority.load();
                    priority = std::min(ownerPriority, nextWaiter->priority.load());
                } while (ownerPriority != priority && nextOwner->priority.compare_exchange_strong(ownerPriority, priority));

                __atomic_store_n(mutex, nextOwner->handle | HandleWaitMask, __ATOMIC_SEQ_CST);
            } else {
                __atomic_store_n(mutex, nextOwner->handle, __ATOMIC_SEQ_CST);
            }

            // Finally, schedule the next owner accordingly
            state.scheduler->InsertThread(nextOwner, false);
        } else {
            __atomic_store_n(mutex, 0, __ATOMIC_SEQ_CST);
            return;
        }
    }

    bool KProcess::ConditionalVariableWait(void *conditional, u32 *mutex, u64 timeout) {
        /* FIXME
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
         */
        return false;
    }

    void KProcess::ConditionalVariableSignal(void *conditional, u64 amount) {
        /* FIXME
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
         */
    }
}
