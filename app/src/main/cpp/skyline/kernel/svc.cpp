#include "svc.h"
#include <os.h>

namespace skyline::kernel::svc {
    void SetHeapSize(DeviceState &state) {
        const u32 size = state.ctx->registers.w1;
        if (size % constant::HeapSizeDiv != 0) {
            state.ctx->registers.x1 = 0;
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcSetHeapSize: 'size' not divisible by 2MB: {}", size);
            return;
        }
        auto& heap = state.process->heap;
        heap->Resize(size);
        state.ctx->registers.w0 = constant::status::Success;
        state.ctx->registers.x1 = heap->address;
        state.logger->Debug("svcSetHeapSize: Allocated at 0x{:X} for 0x{:X} bytes", heap->address, heap->size);
    }

    void SetMemoryAttribute(DeviceState &state) {
        const u64 address = state.ctx->registers.x0;
        if (!utils::PageAligned(address)) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcSetMemoryAttribute: 'address' not page aligned: 0x{:X}", address);
            return;
        }
        const u64 size = state.ctx->registers.x1;
        if (!utils::PageAligned(size)) {
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcSetMemoryAttribute: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }
        memory::MemoryAttribute mask{.value = state.ctx->registers.w2};
        memory::MemoryAttribute value{.value = state.ctx->registers.w3};
        u32 maskedValue = mask.value | value.value;
        if (maskedValue != mask.value || !mask.isUncached || mask.isDeviceShared || mask.isBorrowed || mask.isIpcLocked) {
            state.ctx->registers.w0 = constant::status::InvCombination;
            state.logger->Warn("svcSetMemoryAttribute: 'mask' invalid: 0x{:X}, 0x{:X}", mask.value, value.value);
            return;
        }
        auto chunk = state.os->memory.GetChunk(address);
        auto block = state.os->memory.GetBlock(address);
        if (!chunk || !block) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcSetMemoryAttribute: Cannot find memory region: 0x{:X}", address);
            return;
        }
        if(!chunk->state.AttributeChangeAllowed) {
            state.ctx->registers.w0 = constant::status::InvState;
            state.logger->Warn("svcSetMemoryAttribute: Attribute change not allowed for chunk: 0x{:X}", address);
            return;
        }
        block->attributes.isUncached = value.isUncached;
        MemoryManager::InsertBlock(chunk, *block);
        state.logger->Debug("svcSetMemoryAttribute: Set caching to {} at 0x{:X} for 0x{:X} bytes", !block->attributes.isUncached, address, size);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void MapMemory(DeviceState &state) {
        const u64 destination = state.ctx->registers.x0;
        const u64 source = state.ctx->registers.x1;
        const u64 size = state.ctx->registers.x2;
        if(!utils::PageAligned(destination) || !utils::PageAligned(source)) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcMapMemory: Addresses not page aligned: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }
        if(!utils::PageAligned(size)) {
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcMapMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }
        auto stack = state.os->memory.GetRegion(memory::Regions::Stack);
        if(!stack.IsInside(destination)) {
            state.ctx->registers.w0 = constant::status::InvMemRange;
            state.logger->Warn("svcMapMemory: Addresses not within stack region: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }
        auto descriptor = state.os->memory.Get(source);
        if(!descriptor) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcMapMemory: Source has no descriptor: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes)", source, destination, size);
            return;
        }
        if(!descriptor->chunk.state.MapAllowed) {
            state.ctx->registers.w0 = constant::status::InvState;
            state.logger->Warn("svcMapMemory: Source doesn't allow usage of svcMapMemory: Source: 0x{:X}, Destination: 0x{:X} (Size: 0x{:X} bytes) 0x{:X}", source, destination, size, descriptor->chunk.state.value);
            return;
        }
        state.process->NewHandle<type::KPrivateMemory>(destination, size, memory::Permission{true, true, true}, memory::MemoryStates::Stack);
        state.process->CopyMemory(source, destination, size);
        auto object = state.process->GetMemoryObject(source);
        if(!object)
            throw exception("svcMapMemory: Cannot find memory object in handle table for address 0x{:X}", source);
        object->UpdatePermission(source, size, {false, false, false});
        state.logger->Debug("svcMapMemory: Mapped range 0x{:X} - 0x{:X} to 0x{:X} - 0x{:X} (Size: 0x{:X} bytes)", source, source + size, destination, destination + size, size);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void QueryMemory(DeviceState &state) {
        u64 address = state.ctx->registers.x2;
        memory::MemoryInfo memInfo{};
        auto descriptor = state.os->memory.Get(address);
        if (descriptor) {
            memInfo = {
                .address = descriptor->block.address,
                .size = descriptor->block.size,
                .type = static_cast<u32>(descriptor->chunk.state.type),
                .attributes = descriptor->block.attributes,
                .r = descriptor->block.permission.r,
                .w = descriptor->block.permission.w,
                .x = descriptor->block.permission.x,
                .deviceRefCount = 0,
                .ipcRefCount = 0,
            };
        } else {
            memInfo = {
                .address = constant::BaseEnd,
                .size = ~(constant::BaseEnd - 1),
                .type = static_cast<u32>(memory::MemoryType::Unmapped)
            };
            state.logger->Debug("svcQueryMemory: Cannot find block of address: 0x{:X}", address);
        }
        state.logger->Debug("svcQueryMemory: Address: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Is Uncached: {}, Permissions: {}{}{}", memInfo.address, memInfo.size, memInfo.type, static_cast<bool>(memInfo.attributes.isUncached), memInfo.r ? "R" : "-", memInfo.w ? "W" : "-", memInfo.x ? "X" : "-");
        state.process->WriteMemory(memInfo, state.ctx->registers.x0);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void ExitProcess(DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting current process: {}", state.process->pid);
        state.os->KillThread(state.process->pid);
    }

    void CreateThread(DeviceState &state) {
        u64 entryAddress = state.ctx->registers.x1;
        u64 entryArgument = state.ctx->registers.x2;
        u64 stackTop = state.ctx->registers.x3;
        u8 priority = static_cast<u8>(state.ctx->registers.w4);
        if ((priority < constant::PriorityNin.first) || (priority > constant::PriorityNin.second)) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcCreateThread: 'priority' invalid: {}", priority);
            return;
        }
        auto thread = state.process->CreateThread(entryAddress, entryArgument, stackTop, priority);
        state.logger->Debug("svcCreateThread: Created thread with handle 0x{:X} (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, PID: {})", thread->handle, entryAddress, entryArgument, stackTop, priority, thread->pid);
        state.ctx->registers.w1 = thread->handle;
        state.ctx->registers.w0 = constant::status::Success;
    }

    void StartThread(DeviceState &state) {
        auto handle = state.ctx->registers.w0;
        try {
            auto thread = state.process->GetHandle<type::KThread>(handle);
            state.logger->Debug("svcStartThread: Starting thread: 0x{:X}, PID: {}", handle, thread->pid);
            thread->Start();
        } catch (const std::exception &) {
            state.logger->Warn("svcStartThread: 'handle' invalid: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void ExitThread(DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting current thread: {}", state.thread->pid);
        state.os->KillThread(state.thread->pid);
    }

    void SleepThread(DeviceState &state) {
        auto in = state.ctx->registers.x0;
        switch (in) {
            case 0:
            case 1:
            case 2:
                state.logger->Debug("svcSleepThread: Yielding thread: {}", in);
                break;
            default:
                struct timespec spec = {
                    .tv_sec = static_cast<time_t>(state.ctx->registers.x0 / 1000000000),
                    .tv_nsec = static_cast<long>(state.ctx->registers.x0 % 1000000000)
                };
                nanosleep(&spec, nullptr);
                state.logger->Debug("svcSleepThread: Thread sleeping for {} ns", in);
        }
    }

    void GetThreadPriority(DeviceState &state) {
        auto handle = state.ctx->registers.w0;
        try {
            auto priority = state.process->GetHandle<type::KThread>(handle)->priority;
            state.ctx->registers.w1 = priority;
            state.ctx->registers.w0 = constant::status::Success;
            state.logger->Debug("svcGetThreadPriority: Writing thread priority {}", priority);
        } catch (const std::exception &) {
            state.logger->Warn("svcGetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void SetThreadPriority(DeviceState &state) {
        auto handle = state.ctx->registers.w0;
        auto priority = state.ctx->registers.w1;
        try {
            state.process->GetHandle<type::KThread>(handle)->UpdatePriority(static_cast<u8>(priority));
            state.ctx->registers.w0 = constant::status::Success;
            state.logger->Debug("svcSetThreadPriority: Setting thread priority to {}", priority);
        } catch (const std::exception &) {
            state.logger->Warn("svcSetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void MapSharedMemory(DeviceState &state) {
        try {
            auto object = state.process->GetHandle<type::KSharedMemory>(state.ctx->registers.w0);
            u64 address = state.ctx->registers.x1;
            if (!utils::PageAligned(address)) {
                state.ctx->registers.w0 = constant::status::InvAddress;
                state.logger->Warn("svcMapSharedMemory: 'address' not page aligned: 0x{:X}", address);
                return;
            }
            const u64 size = state.ctx->registers.x2;
            if (!utils::PageAligned(size)) {
                state.ctx->registers.w0 = constant::status::InvSize;
                state.logger->Warn("svcMapSharedMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
                return;
            }
            u32 perm = state.ctx->registers.w3;
            memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
            if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
                state.logger->Warn("svcMapSharedMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
                state.ctx->registers.w0 = constant::status::InvPermission;
                return;
            }
            state.logger->Debug("svcMapSharedMemory: Mapping shared memory at 0x{:X} for {} bytes ({}{}{})", address, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            object->Map(address, size, permission);
            state.ctx->registers.w0 = constant::status::Success;
        } catch (const std::exception &) {
            state.logger->Warn("svcMapSharedMemory: 'handle' invalid: 0x{:X}", state.ctx->registers.w0);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void CreateTransferMemory(DeviceState &state) {
        u64 address = state.ctx->registers.x1;
        if (!utils::PageAligned(address)) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcCreateTransferMemory: 'address' not page aligned: 0x{:X}", address);
            return;
        }
        u64 size = state.ctx->registers.x2;
        if (!utils::PageAligned(size)) {
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcCreateTransferMemory: 'size' {}: 0x{:X}", size ? "not page aligned" : "is zero", size);
            return;
        }
        u32 perm = state.ctx->registers.w3;
        memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
        if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
            state.logger->Warn("svcCreateTransferMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            state.ctx->registers.w0 = constant::status::InvPermission;
            return;
        }
        state.logger->Debug("svcCreateTransferMemory: Creating transfer memory at 0x{:X} for {} bytes ({}{}{})", address, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
        auto shmem = state.process->NewHandle<type::KTransferMemory>(state.process->pid, address, size, permission);
        state.ctx->registers.w0 = constant::status::Success;
        state.ctx->registers.w1 = shmem.handle;
    }

    void CloseHandle(DeviceState &state) {
        auto handle = static_cast<handle_t>(state.ctx->registers.w0);
        try {
            state.process->handles.erase(handle);
            state.logger->Debug("svcCloseHandle: Closing handle: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::Success;
        } catch (const std::exception &) {
            state.logger->Warn("svcCloseHandle: 'handle' invalid: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void ResetSignal(DeviceState &state) {
        auto handle = state.ctx->registers.w0;
        try {
            auto &object = state.process->handles.at(handle);
            switch (object->objectType) {
                case (type::KType::KEvent):
                    std::static_pointer_cast<type::KEvent>(object)->ResetSignal();
                    break;
                case (type::KType::KProcess):
                    std::static_pointer_cast<type::KProcess>(object)->ResetSignal();
                    break;
                default: {
                    state.logger->Warn("svcResetSignal: 'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    state.ctx->registers.w0 = constant::status::InvHandle;
                    return;
                }
            }
            state.logger->Debug("svcResetSignal: Resetting signal: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::Success;
        } catch (const std::out_of_range &) {
            state.logger->Warn("svcResetSignal: 'handle' invalid: 0x{:X}", handle);
            state.ctx->registers.w0 = constant::status::InvHandle;
            return;
        }
    }

    void WaitSynchronization(DeviceState &state) {
        auto numHandles = state.ctx->registers.w2;
        if (numHandles > constant::MaxSyncHandles) {
            state.ctx->registers.w0 = constant::status::MaxHandles;
            return;
        }
        std::vector<handle_t> waitHandles(numHandles);
        std::vector<std::shared_ptr<type::KSyncObject>> objectTable;
        state.process->ReadMemory(waitHandles.data(), state.ctx->registers.x1, numHandles * sizeof(handle_t));
        std::string handleStr;
        for (const auto &handle : waitHandles) {
            handleStr += fmt::format("* 0x{:X}\n", handle);
            auto object = state.process->handles.at(handle);
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    break;
                default: {
                    state.ctx->registers.w0 = constant::status::InvHandle;
                    return;
                }
            }
            objectTable.push_back(std::static_pointer_cast<type::KSyncObject>(object));
        }
        auto timeout = state.ctx->registers.x3;
        state.logger->Debug("svcWaitSynchronization: Waiting on handles:\n{}Timeout: 0x{:X} ns", handleStr, timeout);
        auto start = utils::GetCurrTimeNs();
        while (true) {
            uint index{};
            for (const auto &object : objectTable) {
                if (object->signalled) {
                    state.logger->Debug("svcWaitSynchronization: Signalled handle: 0x{:X}", waitHandles.at(index));
                    state.ctx->registers.w0 = constant::status::Success;
                    state.ctx->registers.w1 = index;
                    return;
                }
                index++;
            }
            if ((utils::GetCurrTimeNs() - start) >= timeout) {
                state.logger->Debug("svcWaitSynchronization: Wait has timed out");
                state.ctx->registers.w0 = constant::status::Timeout;
                return;
            }
        }
    }

    void ArbitrateLock(DeviceState &state) {
        auto addr = state.ctx->registers.x1;
        if ((addr & ((1UL << WORD_BIT) - 1U))) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcArbitrateLock: 'address' not word aligned: 0x{:X}", addr);
            return;
        }
        auto handle = state.ctx->registers.w2;
        if (handle != state.thread->handle)
            throw exception("svcArbitrateLock: Called from another thread");
        state.logger->Debug("svcArbitrateLock: Locking mutex at 0x{:X} for thread 0x{:X}", addr, handle);
        state.process->MutexLock(addr);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void ArbitrateUnlock(DeviceState &state) {
        auto address = state.ctx->registers.x0;
        if ((address & ((1UL << WORD_BIT) - 1U))) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcArbitrateUnlock: 'address' not word aligned: 0x{:X}", address);
            return;
        }
        state.logger->Debug("svcArbitrateUnlock: Unlocking mutex at 0x{:X}", address);
        state.process->MutexUnlock(address);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void WaitProcessWideKeyAtomic(DeviceState &state) {
        auto mtxAddress = state.ctx->registers.x0;
        auto condAddress = state.ctx->registers.x1;
        try {
            auto &cvar = state.process->condVars.at(condAddress);
            if ((mtxAddress & ((1UL << WORD_BIT) - 1U))) {
                state.ctx->registers.w0 = constant::status::InvAddress;
                state.logger->Warn("svcWaitProcessWideKeyAtomic: mutex address not word aligned: 0x{:X}", mtxAddress);
                return;
            }
            auto handle = state.ctx->registers.w2;
            if (handle != state.thread->handle)
                throw exception("svcWaitProcessWideKeyAtomic: Called from another thread");
            state.process->MutexLock(mtxAddress);
            auto &mutex = state.process->mutexes.at(mtxAddress);
            auto timeout = state.ctx->registers.x3;
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Mutex: 0x{:X}, Conditional-Variable: 0x:{:X}, Timeout: {} ns", mtxAddress, condAddress, timeout);
            timespec spec{};
            clock_gettime(CLOCK_REALTIME, &spec);
            u128 time = u128(spec.tv_sec * 1000000000U + spec.tv_nsec) + timeout; // u128 to prevent overflow
            spec.tv_sec = static_cast<time_t>(time / 1000000000U);
            spec.tv_nsec = static_cast<long>(time % 1000000000U);
            if (pthread_cond_timedwait(&cvar, &mutex, &spec) == ETIMEDOUT)
                state.ctx->registers.w0 = constant::status::Timeout;
            else
                state.ctx->registers.w0 = constant::status::Success;
            state.process->MutexUnlock(mtxAddress);
        } catch (const std::out_of_range &) {
            state.logger->Debug("svcWaitProcessWideKeyAtomic: No Conditional-Variable at 0x{:X}", condAddress);
            state.process->condVars[condAddress] = PTHREAD_COND_INITIALIZER;
            state.ctx->registers.w0 = constant::status::Success;
        }
    }

    void SignalProcessWideKey(DeviceState &state) {
        auto address = state.ctx->registers.x0;
        auto count = state.ctx->registers.w1;
        try {
            state.logger->Debug("svcSignalProcessWideKey: Signalling Conditional-Variable at 0x{:X} for {}", address, count);
            auto &cvar = state.process->condVars.at(address);
            if (count == UINT32_MAX)
                pthread_cond_broadcast(&cvar);
            else
                for (u32 iter = 0; iter < count; iter++)
                    pthread_cond_signal(&cvar);
        } catch (const std::out_of_range &) {
            state.logger->Debug("svcSignalProcessWideKey: No Conditional-Variable at 0x{:X}", address);
            state.process->condVars[address] = PTHREAD_COND_INITIALIZER;
        }
        state.ctx->registers.w0 = constant::status::Success;
    }

    void GetSystemTick(DeviceState &state) {
        u64 tick;
        asm("STR X1, [SP, #-16]!\n\t"
            "MRS %0, CNTVCT_EL0\n\t"
            "MOV X1, #0xF800\n\t"
            "MOVK X1, #0x124, lsl #16\n\t"
            "MUL %0, %0, X1\n\t"
            "MRS X1, CNTFRQ_EL0\n\t"
            "UDIV %0, %0, X1\n\t"
            "LDR X1, [SP], #16" : "=r"(tick));
        state.ctx->registers.x0 = tick;
    }

    void ConnectToNamedPort(DeviceState &state) {
        char port[constant::PortSize + 1]{0};
        state.process->ReadMemory(port, state.ctx->registers.x1, constant::PortSize);
        handle_t handle{};
        if (std::strcmp(port, "sm:") == 0)
            handle = state.os->serviceManager.NewSession(service::Service::sm);
        else {
            state.logger->Warn("svcConnectToNamedPort: Connecting to invalid port: '{}'", port);
            state.ctx->registers.w0 = constant::status::NotFound;
            return;
        }
        state.logger->Debug("svcConnectToNamedPort: Connecting to port '{}' at 0x{:X}", port, handle);
        state.ctx->registers.w1 = handle;
        state.ctx->registers.w0 = constant::status::Success;
    }

    void SendSyncRequest(DeviceState &state) {
        state.os->serviceManager.SyncRequestHandler(static_cast<handle_t>(state.ctx->registers.x0));
        state.ctx->registers.w0 = constant::status::Success;
    }

    void GetThreadId(DeviceState &state) {
        pid_t pid{};
        auto handle = state.ctx->registers.w1;
        if (handle != constant::ThreadSelf) {
            pid = state.process->GetHandle<type::KThread>(handle)->pid;
        } else
            pid = state.thread->pid;
        state.logger->Debug("svcGetThreadId: Handle: 0x{:X}, PID: {}", handle, pid);
        state.ctx->registers.x1 = static_cast<u64>(pid);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void OutputDebugString(DeviceState &state) {
        std::string debug(state.ctx->registers.x1, '\0');
        state.process->ReadMemory(debug.data(), state.ctx->registers.x0, state.ctx->registers.x1);
        if (debug.back() == '\n')
            debug.pop_back();
        state.logger->Info("Debug Output: {}", debug);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void GetInfo(DeviceState &state) {
        auto id0 = state.ctx->registers.w1;
        auto handle = state.ctx->registers.w2;
        auto id1 = state.ctx->registers.x3;
        u64 out{};
        switch (id0) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
            case constant::infoState::TitleId:
            case constant::infoState::PrivilegedProcessId:
                break;
            case constant::infoState::AliasRegionBaseAddr:
                out = state.os->memory.GetRegion(memory::Regions::Alias).address;
                break;
            case constant::infoState::AliasRegionSize:
                out = state.os->memory.GetRegion(memory::Regions::Alias).size;
                break;
            case constant::infoState::HeapRegionBaseAddr:
                out = state.os->memory.GetRegion(memory::Regions::Heap).address;
                break;
            case constant::infoState::HeapRegionSize:
                out = state.os->memory.GetRegion(memory::Regions::Heap).size;
                break;
            case constant::infoState::TotalMemoryAvailable:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::TotalMemoryUsage:
                out = state.process->heap->address + constant::DefStackSize + state.os->memory.GetProgramSize();
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                out = constant::BaseAddress;
                break;
            case constant::infoState::AddressSpaceSize:
                out = constant::BaseEnd;
                break;
            case constant::infoState::StackRegionBaseAddr:
                out = state.os->memory.GetRegion(memory::Regions::Stack).address;
                break;
            case constant::infoState::StackRegionSize:
                out = state.os->memory.GetRegion(memory::Regions::Stack).size;
                break;
            case constant::infoState::PersonalMmHeapSize:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::PersonalMmHeapUsage:
                out = state.process->heap->address + constant::DefStackSize;
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                out = constant::TotalPhyMem; // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                out = state.process->heap->size + constant::DefStackSize; // TODO: Same as above
                break;
            case constant::infoState::UserExceptionContextAddr:
                out = state.process->tlsPages[0]->Get(0);
                break;
            default:
                state.logger->Warn("svcGetInfo: Unimplemented case ID0: {}, ID1: {}", id0, id1);
                state.ctx->registers.w0 = constant::status::Unimpl;
                return;
        }
        state.logger->Debug("svcGetInfo: ID0: {}, ID1: {}, Out: 0x{:X}", id0, id1, out);
        state.ctx->registers.x1 = out;
        state.ctx->registers.w0 = constant::status::Success;
    }
}
