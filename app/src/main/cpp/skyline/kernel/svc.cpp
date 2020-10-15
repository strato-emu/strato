// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <kernel/types/KProcess.h>
#include "results.h"
#include "svc.h"

namespace skyline::kernel::svc {
    void SetHeapSize(const DeviceState &state) {
        auto size{state.ctx->gpr.w1};

        if (!util::IsAligned(size, 0x200000)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.ctx->gpr.x1 = 0;

            state.logger->Warn("svcSetHeapSize: 'size' not divisible by 2MB: {}", size);
            return;
        }

        auto &heap{state.process->heap};
        heap->Resize(size);

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.x1 = reinterpret_cast<u64>(heap->ptr);

        state.logger->Debug("svcSetHeapSize: Allocated at {} for 0x{:X} bytes", heap->ptr, heap->size);
    }

    void SetMemoryAttribute(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x0)};
        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcSetMemoryAttribute: 'pointer' not page aligned: 0x{:X}", pointer);
            return;
        }

        auto size{state.ctx->gpr.x1};
        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcSetMemoryAttribute: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        memory::MemoryAttribute mask{.value = state.ctx->gpr.w2};
        memory::MemoryAttribute value{.value = state.ctx->gpr.w3};

        auto maskedValue{mask.value | value.value};
        if (maskedValue != mask.value || !mask.isUncached || mask.isDeviceShared || mask.isBorrowed || mask.isIpcLocked) {
            state.ctx->gpr.w0 = result::InvalidCombination;
            state.logger->Warn("svcSetMemoryAttribute: 'mask' invalid: 0x{:X}, 0x{:X}", mask.value, value.value);
            return;
        }

        auto chunk{state.process->memory.Get(pointer)};
        if (!chunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcSetMemoryAttribute: Cannot find memory region: 0x{:X}", pointer);
            return;
        }

        if (!chunk->state.attributeChangeAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcSetMemoryAttribute: Attribute change not allowed for chunk: 0x{:X}", pointer);
            return;
        }

        auto newChunk{*chunk};
        newChunk.ptr = pointer;
        newChunk.size = size;
        newChunk.attributes.isUncached = value.isUncached;
        state.process->memory.InsertChunk(newChunk);

        state.logger->Debug("svcSetMemoryAttribute: Set caching to {} at 0x{:X} for 0x{:X} bytes", static_cast<bool>(value.isUncached), pointer, size);
        state.ctx->gpr.w0 = Result{};
    }

    void MapMemory(const DeviceState &state) {
        auto destination{reinterpret_cast<u8*>(state.ctx->gpr.x0)};
        auto source{reinterpret_cast<u8*>(state.ctx->gpr.x1)};
        auto size{state.ctx->gpr.x2};

        if (!util::PageAligned(destination) || !util::PageAligned(source)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcMapMemory: Addresses not page aligned: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcMapMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        auto stack{state.process->memory.stack};
        if (!stack.IsInside(destination)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            state.logger->Warn("svcMapMemory: Destination not within stack region: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        auto chunk{state.process->memory.Get(source)};
        if (!chunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcMapMemory: Source has no descriptor: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }
        if (!chunk->state.mapAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcMapMemory: Source doesn't allow usage of svcMapMemory: Source: 0x{:X}, Destination: 0x{:X}, Size: 0x{:X}, MemoryState: 0x{:X}", source, destination, size, chunk->state.value);
            return;
        }

        state.process->NewHandle<type::KPrivateMemory>(destination, size, chunk->permission, memory::states::Stack);
        memcpy(destination, source, size);

        auto object{state.process->GetMemoryObject(source)};
        if (!object)
            throw exception("svcMapMemory: Cannot find memory object in handle table for address 0x{:X}", source);
        object->item->UpdatePermission(source, size, {false, false, false});

        state.logger->Debug("svcMapMemory: Mapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void UnmapMemory(const DeviceState &state) {
        auto source{reinterpret_cast<u8*>(state.ctx->gpr.x0)};
        auto destination{reinterpret_cast<u8*>(state.ctx->gpr.x1)};
        auto size{state.ctx->gpr.x2};

        if (!util::PageAligned(destination) || !util::PageAligned(source)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcUnmapMemory: Addresses not page aligned: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcUnmapMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        auto stack{state.process->memory.stack};
        if (!stack.IsInside(source)) {
            state.ctx->gpr.w0 = result::InvalidMemoryRegion;
            state.logger->Warn("svcUnmapMemory: Source not within stack region: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        auto sourceChunk{state.process->memory.Get(source)};
        auto destChunk{state.process->memory.Get(destination)};
        if (!sourceChunk || !destChunk) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcUnmapMemory: Addresses have no descriptor: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }

        if (!destChunk->state.mapAllowed) {
            state.ctx->gpr.w0 = result::InvalidState;
            state.logger->Warn("svcUnmapMemory: Destination doesn't allow usage of svcMapMemory: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes) 0x{:X}", source, destination, size, destChunk->state.value);
            return;
        }

        auto destObject{state.process->GetMemoryObject(destination)};
        if (!destObject)
            throw exception("svcUnmapMemory: Cannot find destination memory object in handle table for address 0x{:X}", destination);

        destObject->item->UpdatePermission(destination, size, sourceChunk->permission);

        std::memcpy(source, destination, size);

        auto sourceObject{state.process->GetMemoryObject(destination)};
        if (!sourceObject)
            throw exception("svcUnmapMemory: Cannot find source memory object in handle table for address 0x{:X}", source);

        state.process->CloseHandle(sourceObject->handle);

        state.logger->Debug("svcUnmapMemory: Unmapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->gpr.w0 = Result{};
    }

    void QueryMemory(const DeviceState &state) {
        memory::MemoryInfo memInfo{};

        auto pointer{reinterpret_cast<u8*>(state.ctx->gpr.x2)};
        auto chunk{state.process->memory.Get(pointer)};

        if (chunk) {
            memInfo = {
                .address = reinterpret_cast<u64>(chunk->ptr),
                .size = chunk->size,
                .type = static_cast<u32>(chunk->state.type),
                .attributes = chunk->attributes.value,
                .permissions = static_cast<u32>(chunk->permission.Get()),
                .deviceRefCount = 0,
                .ipcRefCount = 0,
            };

            state.logger->Debug("svcQueryMemory: Address: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Is Uncached: {}, Permissions: {}{}{}", memInfo.address, memInfo.size, memInfo.type, static_cast<bool>(chunk->attributes.isUncached), chunk->permission.r ? 'R' : '-', chunk->permission.w ? 'W' : '-', chunk->permission.x ? 'X' : '-');
        } else {
            auto addressSpaceEnd{reinterpret_cast<u64>(state.process->memory.addressSpace.address + state.process->memory.addressSpace.size)};

            memInfo = {
                .address = addressSpaceEnd,
                .size = ~addressSpaceEnd + 1,
                .type = static_cast<u32>(memory::MemoryType::Reserved),
            };

            state.logger->Debug("svcQueryMemory: Trying to query memory outside of the application's address space: 0x{:X}", pointer);
        }

        *reinterpret_cast<memory::MemoryInfo*>(state.ctx->gpr.x0) = memInfo;

        state.ctx->gpr.w0 = Result{};
    }

    void ExitProcess(const DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting process");
        //state.os->KillThread(state.process->pid);
    }

    void CreateThread(const DeviceState &state) {
        auto entry{reinterpret_cast<void*>(state.ctx->gpr.x1)};
        auto entryArgument{state.ctx->gpr.x2};
        auto stackTop{reinterpret_cast<u8*>(state.ctx->gpr.x3)};
        auto priority{static_cast<i8>(state.ctx->gpr.w4)};

        if (!constant::HosPriority.Valid(priority)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcCreateThread: 'priority' invalid: {}", priority);
            return;
        }

        auto stack{state.process->GetMemoryObject(stackTop)};
        if (!stack)
            throw exception("svcCreateThread: Cannot find memory object in handle table for thread stack: 0x{:X}", stackTop);

        auto thread{state.process->CreateThread(entry, entryArgument, priority, static_pointer_cast<type::KPrivateMemory>(stack->item))};
        state.logger->Debug("svcCreateThread: Created thread with handle 0x{:X} (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, ID: {})", thread->handle, entry, entryArgument, stackTop, priority, thread->id);

        state.ctx->gpr.w1 = thread->handle;
        state.ctx->gpr.w0 = Result{};
    }

    void StartThread(const DeviceState &state) {
        auto handle{state.ctx->gpr.w0};
        try {
            auto thread{state.process->GetHandle<type::KThread>(handle)};
            state.logger->Debug("svcStartThread: Starting thread: 0x{:X}, PID: {}", handle, thread->id);
            thread->Start();
            state.ctx->gpr.w0 = Result{};
        } catch (const std::exception &) {
            state.logger->Warn("svcStartThread: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ExitThread(const DeviceState &state) {
        state.logger->Debug("svcExitThread: Exiting current thread: {}", state.thread->id);
        state.os->KillThread(state.thread->id);
    }

    void SleepThread(const DeviceState &state) {
        auto in{state.ctx->gpr.x0};

        switch (in) {
            case 0:
            case 1:
            case 2:
                state.logger->Debug("svcSleepThread: Yielding thread: {}", in);
                break;
            default:
                state.logger->Debug("svcSleepThread: Thread sleeping for {} ns", in);
                struct timespec spec{
                    .tv_sec = static_cast<time_t>(state.ctx->gpr.x0 / 1000000000),
                    .tv_nsec = static_cast<long>(state.ctx->gpr.x0 % 1000000000)
                };
                nanosleep(&spec, nullptr);
        }
    }

    void GetThreadPriority(const DeviceState &state) {
        auto handle{state.ctx->gpr.w1};
        try {
            auto priority{state.process->GetHandle<type::KThread>(handle)->priority};
            state.logger->Debug("svcGetThreadPriority: Writing thread priority {}", priority);

            state.ctx->gpr.w1 = priority;
            state.ctx->gpr.w0 = Result{};
        } catch (const std::exception &) {
            state.logger->Warn("svcGetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void SetThreadPriority(const DeviceState &state) {
        auto handle{state.ctx->gpr.w0};
        auto priority{state.ctx->gpr.w1};

        try {
            state.logger->Debug("svcSetThreadPriority: Setting thread priority to {}", priority);
            state.process->GetHandle<type::KThread>(handle)->UpdatePriority(static_cast<u8>(priority));
            state.ctx->gpr.w0 = Result{};
        } catch (const std::exception &) {
            state.logger->Warn("svcSetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ClearEvent(const DeviceState &state) {
        auto object{state.process->GetHandle<type::KEvent>(state.ctx->gpr.w0)};
        object->signalled = false;
        state.ctx->gpr.w0 = Result{};
    }

    void MapSharedMemory(const DeviceState &state) {
        try {
            auto object{state.process->GetHandle<type::KSharedMemory>(state.ctx->gpr.w0)};
            auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};

            if (!util::PageAligned(pointer)) {
                state.ctx->gpr.w0 = result::InvalidAddress;
                state.logger->Warn("svcMapSharedMemory: 'pointer' not page aligned: 0x{:X}", pointer);
                return;
            }

            auto size{state.ctx->gpr.x2};
            if (!util::PageAligned(size)) {
                state.ctx->gpr.w0 = result::InvalidSize;
                state.logger->Warn("svcMapSharedMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
                return;
            }

            auto permission{*reinterpret_cast<memory::Permission *>(&state.ctx->gpr.w3)};
            if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
                state.logger->Warn("svcMapSharedMemory: 'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
                state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
                return;
            }

            state.logger->Debug("svcMapSharedMemory: Mapping shared memory at 0x{:X} for {} bytes ({}{}{})", pointer, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

            object->Map(pointer, size, permission);

            state.ctx->gpr.w0 = Result{};
        } catch (const std::exception &) {
            state.logger->Warn("svcMapSharedMemory: 'handle' invalid: 0x{:X}", state.ctx->gpr.w0);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void CreateTransferMemory(const DeviceState &state) {
        auto pointer{reinterpret_cast<u8 *>(state.ctx->gpr.x1)};
        if (!util::PageAligned(pointer)) {
            state.ctx->gpr.w0 = result::InvalidAddress;
            state.logger->Warn("svcCreateTransferMemory: 'pointer' not page aligned: 0x{:X}", pointer);
            return;
        }

        auto size{state.ctx->gpr.x2};
        if (!util::PageAligned(size)) {
            state.ctx->gpr.w0 = result::InvalidSize;
            state.logger->Warn("svcCreateTransferMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }

        auto permission{*reinterpret_cast<memory::Permission *>(&state.ctx->gpr.w3)};
        if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
            state.logger->Warn("svcCreateTransferMemory: 'permission' invalid: {}{}{}", permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');
            state.ctx->gpr.w0 = result::InvalidNewMemoryPermission;
            return;
        }

        auto tmem{state.process->NewHandle<type::KTransferMemory>(pointer, size, permission)};
        state.logger->Debug("svcCreateTransferMemory: Creating transfer memory at 0x{:X} for {} bytes ({}{}{})", pointer, size, permission.r ? 'R' : '-', permission.w ? 'W' : '-', permission.x ? 'X' : '-');

        state.ctx->gpr.w0 = Result{};
        state.ctx->gpr.w1 = tmem.handle;
    }

    void CloseHandle(const DeviceState &state) {
        auto handle{static_cast<KHandle>(state.ctx->gpr.w0)};
        try {
            state.process->CloseHandle(handle);
            state.logger->Debug("svcCloseHandle: Closing handle: 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::exception &) {
            state.logger->Warn("svcCloseHandle: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ResetSignal(const DeviceState &state) {
        auto handle{state.ctx->gpr.w0};
        try {
            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KEvent:
                    std::static_pointer_cast<type::KEvent>(object)->ResetSignal();
                    break;

                case type::KType::KProcess:
                    std::static_pointer_cast<type::KProcess>(object)->ResetSignal();
                    break;

                default: {
                    state.logger->Warn("svcResetSignal: 'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }

            state.logger->Debug("svcResetSignal: Resetting signal: 0x{:X}", handle);
            state.ctx->gpr.w0 = Result{};
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcResetSignal: 'handle' invalid: 0x{:X}", handle);
            state.ctx->gpr.w0 = result::InvalidHandle;
            return;
        }
    }

    void WaitSynchronization(const DeviceState &state) {
        constexpr u8 maxSyncHandles{0x40}; // The total amount of handles that can be passed to WaitSynchronization

        auto numHandles{state.ctx->gpr.w2};
        if (numHandles > maxSyncHandles) {
            state.ctx->gpr.w0 = result::OutOfHandles;
            return;
        }

        std::string handleStr;
        std::vector<std::shared_ptr<type::KSyncObject>> objectTable;
        span waitHandles(reinterpret_cast<KHandle*>(state.ctx->gpr.x1), numHandles);

        for (const auto &handle : waitHandles) {
            handleStr += fmt::format("* 0x{:X}\n", handle);

            auto object{state.process->GetHandle(handle)};
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    break;

                default: {
                    state.ctx->gpr.w0 = result::InvalidHandle;
                    return;
                }
            }

            objectTable.push_back(std::static_pointer_cast<type::KSyncObject>(object));
        }

        auto timeout{state.ctx->gpr.x3};
        state.logger->Debug("svcWaitSynchronization: Waiting on handles:\n{}Timeout: 0x{:X} ns", handleStr, timeout);

        auto start{util::GetTimeNs()};
        while (true) {
            if (state.thread->cancelSync) {
                state.thread->cancelSync = false;
                state.ctx->gpr.w0 = result::Cancelled;
                break;
            }

            uint index{};
            for (const auto &object : objectTable) {
                if (object->signalled) {
                    state.logger->Debug("svcWaitSynchronization: Signalled handle: 0x{:X}", waitHandles[index]);
                    state.ctx->gpr.w0 = Result{};
                    state.ctx->gpr.w1 = index;
                    return;
                }
                index++;
            }

            if ((util::GetTimeNs() - start) >= timeout) {
                state.logger->Debug("svcWaitSynchronization: Wait has timed out");
                state.ctx->gpr.w0 = result::TimedOut;
                return;
            }
        }
    }

    void CancelSynchronization(const DeviceState &state) {
        try {
            state.process->GetHandle<type::KThread>(state.ctx->gpr.w0)->cancelSync = true;
        } catch (const std::exception &) {
            state.logger->Warn("svcCancelSynchronization: 'handle' invalid: 0x{:X}", state.ctx->gpr.w0);
            state.ctx->gpr.w0 = result::InvalidHandle;
        }
    }

    void ArbitrateLock(const DeviceState &state) {
        auto pointer{reinterpret_cast<u32*>(state.ctx->gpr.x1)};
        if (!util::WordAligned(pointer)) {
            state.logger->Warn("svcArbitrateLock: 'pointer' not word aligned: 0x{:X}", pointer);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        auto ownerHandle{state.ctx->gpr.w0};
        auto requesterHandle{state.ctx->gpr.w2};
        if (requesterHandle != state.thread->handle)
            throw exception("svcWaitProcessWideKeyAtomic: Handle doesn't match current thread: 0x{:X} for thread 0x{:X}", requesterHandle, state.thread->handle);

        state.logger->Debug("svcArbitrateLock: Locking mutex at 0x{:X}", pointer);

        if (state.process->MutexLock(pointer, ownerHandle))
            state.logger->Debug("svcArbitrateLock: Locked mutex at 0x{:X}", pointer);
        else
            state.logger->Debug("svcArbitrateLock: Owner handle did not match current owner for mutex or didn't have waiter flag at 0x{:X}", pointer);

        state.ctx->gpr.w0 = Result{};
    }

    void ArbitrateUnlock(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32*>(state.ctx->gpr.x0)};
        if (!util::WordAligned(mutex)) {
            state.logger->Warn("svcArbitrateUnlock: 'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        state.logger->Debug("svcArbitrateUnlock: Unlocking mutex at 0x{:X}", mutex);

        if (state.process->MutexUnlock(mutex)) {
            state.logger->Debug("svcArbitrateUnlock: Unlocked mutex at 0x{:X}", mutex);
            state.ctx->gpr.w0 = Result{};
        } else {
            state.logger->Debug("svcArbitrateUnlock: A non-owner thread tried to release a mutex at 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
        }
    }

    void WaitProcessWideKeyAtomic(const DeviceState &state) {
        auto mutex{reinterpret_cast<u32*>(state.ctx->gpr.x0)};
        if (!util::WordAligned(mutex)) {
            state.logger->Warn("svcWaitProcessWideKeyAtomic: 'mutex' not word aligned: 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        auto conditional{reinterpret_cast<void*>(state.ctx->gpr.x1)};
        auto handle{state.ctx->gpr.w2};
        if (handle != state.thread->handle)
            throw exception("svcWaitProcessWideKeyAtomic: Handle doesn't match current thread: 0x{:X} for thread 0x{:X}", handle, state.thread->handle);

        if (!state.process->MutexUnlock(mutex)) {
            state.logger->Debug("WaitProcessWideKeyAtomic: A non-owner thread tried to release a mutex at 0x{:X}", mutex);
            state.ctx->gpr.w0 = result::InvalidAddress;
            return;
        }

        auto timeout{state.ctx->gpr.x3};
        state.logger->Debug("svcWaitProcessWideKeyAtomic: Mutex: 0x{:X}, Conditional-Variable: 0x{:X}, Timeout: {} ns", mutex, conditional, timeout);

        if (state.process->ConditionalVariableWait(conditional, mutex, timeout)) {
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Waited for conditional variable and relocked mutex");
            state.ctx->gpr.w0 = Result{};
        } else {
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Wait has timed out");
            state.ctx->gpr.w0 = result::TimedOut;
        }
    }

    void SignalProcessWideKey(const DeviceState &state) {
        auto conditional{reinterpret_cast<void*>(state.ctx->gpr.x0)};
        auto count{state.ctx->gpr.w1};

        state.logger->Debug("svcSignalProcessWideKey: Signalling Conditional-Variable at 0x{:X} for {}", conditional, count);
        state.process->ConditionalVariableSignal(conditional, count);
        state.ctx->gpr.w0 = Result{};
    }

    void GetSystemTick(const DeviceState &state) {
        u64 tick;
        asm("STR X1, [SP, #-16]!\n\t"
            "MRS %0, CNTVCT_EL0\n\t"
            "MOV X1, #0xF800\n\t"
            "MOVK X1, #0x124, lsl #16\n\t"
            "MUL %0, %0, X1\n\t"
            "MRS X1, CNTFRQ_EL0\n\t"
            "UDIV %0, %0, X1\n\t"
            "LDR X1, [SP], #16" : "=r"(tick));
        state.ctx->gpr.x0 = tick;
    }

    void ConnectToNamedPort(const DeviceState &state) {
        constexpr u8 portSize = 0x8; //!< The size of a port name string
        std::string_view port(span(reinterpret_cast<char*>(state.ctx->gpr.x1), portSize).as_string(true));

        KHandle handle{};
        if (port.compare("sm:") >= 0) {
            handle = state.process->NewHandle<type::KSession>(std::static_pointer_cast<service::BaseService>(state.os->serviceManager.smUserInterface)).handle;
        } else {
            state.logger->Warn("svcConnectToNamedPort: Connecting to invalid port: '{}'", port);
            state.ctx->gpr.w0 = result::NotFound;
            return;
        }

        state.logger->Debug("svcConnectToNamedPort: Connecting to port '{}' at 0x{:X}", port, handle);

        state.ctx->gpr.w1 = handle;
        state.ctx->gpr.w0 = Result{};
    }

    void SendSyncRequest(const DeviceState &state) {
        state.os->serviceManager.SyncRequestHandler(static_cast<KHandle>(state.ctx->gpr.x0));
        state.ctx->gpr.w0 = Result{};
    }

    void GetThreadId(const DeviceState &state) {
        constexpr KHandle threadSelf{0xFFFF8000}; // The handle used by threads to refer to themselves
        auto handle{state.ctx->gpr.w1};
        pid_t pid{};

        if (handle != threadSelf)
            pid = state.process->GetHandle<type::KThread>(handle)->id;
        else
            pid = state.thread->id;

        state.logger->Debug("svcGetThreadId: Handle: 0x{:X}, PID: {}", handle, pid);

        state.ctx->gpr.x1 = static_cast<u64>(pid);
        state.ctx->gpr.w0 = Result{};
    }

    void OutputDebugString(const DeviceState &state) {
        auto debug{span(reinterpret_cast<u8*>(state.ctx->gpr.x0), state.ctx->gpr.x1).as_string()};

        if (debug.back() == '\n')
            debug.remove_suffix(1);

        state.logger->Info("Debug Output: {}", debug);
        state.ctx->gpr.w0 = Result{};
    }

    void GetInfo(const DeviceState &state) {
        auto id0{state.ctx->gpr.w1};
        auto handle{state.ctx->gpr.w2};
        auto id1{state.ctx->gpr.x3};

        u64 out{};

        constexpr u64 totalPhysicalMemory{0xF8000000}; // ~4 GB of RAM

        switch (id0) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
            case constant::infoState::TitleId:
            case constant::infoState::PrivilegedProcessId:
                break;

            case constant::infoState::AliasRegionBaseAddr:
                out = state.process->memory.alias.address;
                break;

            case constant::infoState::AliasRegionSize:
                out = state.process->memory.alias.size;
                break;

            case constant::infoState::HeapRegionBaseAddr:
                out = state.process->memory.heap.address;
                break;

            case constant::infoState::HeapRegionSize:
                out = state.process->memory.heap.size;
                break;

            case constant::infoState::TotalMemoryAvailable:
                out = totalPhysicalMemory;
                break;

            case constant::infoState::TotalMemoryUsage:
                out = state.process->heap->size + state.thread->stack->size + state.process->memory.GetProgramSize();
                break;

            case constant::infoState::AddressSpaceBaseAddr:
                out = state.process->memory.base.address;
                break;

            case constant::infoState::AddressSpaceSize:
                out = state.process->memory.base.size;
                break;

            case constant::infoState::StackRegionBaseAddr:
                out = state.process->memory.stack.address;
                break;

            case constant::infoState::StackRegionSize:
                out = state.process->memory.stack.size;
                break;

            case constant::infoState::PersonalMmHeapSize:
                out = totalPhysicalMemory;
                break;

            case constant::infoState::PersonalMmHeapUsage:
                out = state.process->heap->size + state.thread->stack->size;
                break;

            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                out = totalPhysicalMemory; // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;

            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                out = state.process->heap->size + state.thread->stack->size; // TODO: Same as above
                break;

            case constant::infoState::UserExceptionContextAddr:
                out = reinterpret_cast<u64>(state.process->tlsPages[0]->Get(0));
                break;

            default:
                state.logger->Warn("svcGetInfo: Unimplemented case ID0: {}, ID1: {}", id0, id1);
                state.ctx->gpr.w0 = result::InvalidEnumValue;
                return;
        }

        state.logger->Debug("svcGetInfo: ID0: {}, ID1: {}, Out: 0x{:X}", id0, id1, out);

        state.ctx->gpr.x1 = out;
        state.ctx->gpr.w0 = Result{};
    }
}
