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
        std::shared_ptr<type::KPrivateMemory> heap;
        try {
            heap = state.process->memoryRegionMap.at(memory::Region::Heap);
            heap->Resize(size, true);
        } catch (const exception &) {
            state.logger->Warn("svcSetHeapSize: Falling back to recreating memory");
            state.process->UnmapPrivateRegion(memory::Region::Heap);
            heap = state.process->MapPrivateRegion(constant::HeapAddr, size, {true, true, false}, memory::Type::Heap, memory::Region::Heap).item;
        }
        state.ctx->registers.w0 = constant::status::Success;
        state.ctx->registers.x1 = heap->address;
        state.logger->Debug("svcSetHeapSize: Allocated at 0x{:X} for 0x{:X} bytes", heap->address, heap->size);
    }

    void SetMemoryAttribute(DeviceState &state) {
        const u64 addr = state.ctx->registers.x0;
        if ((addr & (PAGE_SIZE - 1U))) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcSetMemoryAttribute: 'address' not page aligned: {}", addr);
            return;
        }
        const u64 size = state.ctx->registers.x1;
        if ((size & (PAGE_SIZE - 1U)) || !size) {
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcSetMemoryAttribute: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
            return;
        }
        u32 mask = state.ctx->registers.w2;
        u32 value = state.ctx->registers.w3;
        u32 maskedValue = mask | value;
        if (maskedValue != mask) {
            state.ctx->registers.w0 = constant::status::InvCombination;
            state.logger->Warn("svcSetMemoryAttribute: 'mask' invalid: 0x{:X}, 0x{:X}", mask, value);
            return;
        }
        memory::MemoryAttribute attribute = *reinterpret_cast<memory::MemoryAttribute *>(&maskedValue);
        bool found = false;
        for (const auto&[address, region] : state.process->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                bool subFound = false;
                for (auto &subregion : region->regionInfoVec) {
                    if ((address >= subregion.address) && (address < (subregion.address + subregion.size)))
                        subregion.isUncached = attribute.isUncached;
                    subFound = true;
                    break;
                }
                if (!subFound)
                    region->regionInfoVec.emplace_back(addr, size, static_cast<bool>(attribute.isUncached));
                found = true;
                break;
            }
        }
        if (!found) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcSetMemoryAttribute: Cannot find memory region: 0x{:X}", addr);
            return;
        }
        state.logger->Debug("svcSetMemoryAttribute: Set caching to {} at 0x{:X} for 0x{:X} bytes", !attribute.isUncached, addr, size);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void QueryMemory(DeviceState &state) {
        memory::MemoryInfo memInfo{};
        u64 addr = state.ctx->registers.x2 & ~(PAGE_SIZE - 1);
        bool found = false;
        for (const auto&[address, region] : state.process->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                memInfo = region->GetInfo(addr);
                found = true;
                break;
            }
        }
        if (!found) {
            for (const auto &object : state.process->handleTable) {
                if (object.second->objectType == type::KType::KSharedMemory) {
                    const auto &mem = state.process->GetHandle<type::KSharedMemory>(object.first);
                    if (mem->guest.valid()) {
                        if (addr >= mem->guest.address && addr < (mem->guest.address + mem->guest.size)) {
                            memInfo = mem->GetInfo();
                            found = true;
                            break;
                        }
                    }
                } else if (object.second->objectType == type::KType::KTransferMemory) {
                    const auto &mem = state.process->GetHandle<type::KTransferMemory>(object.first);
                    if (addr >= mem->cAddress && addr < (mem->cAddress + mem->cSize)) {
                        memInfo = mem->GetInfo();
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                memInfo = {
                    .baseAddress = constant::BaseAddr,
                    .size = static_cast<u64>(constant::BaseEnd),
                    .type = static_cast<u64>(memory::Type::Unmapped)
                };
                state.logger->Debug("svcQueryMemory: Cannot find block of address: 0x{:X}", addr);
            }
        }
        state.logger->Debug("svcQueryMemory: Address: 0x{:X}, Size: 0x{:X}, Type: 0x{:X}, Is Uncached: {}, Permissions: {}{}{}", memInfo.baseAddress, memInfo.size, memInfo.type, static_cast<bool>(memInfo.memoryAttribute.isUncached), memInfo.r ? "R" : "-", memInfo.w ? "W" : "-", memInfo.x ? "X" : "-");
        state.process->WriteMemory<memory::MemoryInfo>(memInfo, state.ctx->registers.x0);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void ExitProcess(DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting current process: {}", state.process->pid);
        state.os->KillThread(state.process->pid);
    }

    void CreateThread(DeviceState &state) {
        u64 entryAddr = state.ctx->registers.x1;
        u64 entryArg = state.ctx->registers.x2;
        u64 stackTop = state.ctx->registers.x3;
        u8 priority = static_cast<u8>(state.ctx->registers.w4);
        if ((priority < constant::PriorityNin.first) && (priority > constant::PriorityNin.second)) { // NOLINT(misc-redundant-expression)
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcCreateThread: 'priority' invalid: {}", priority);
            return;
        }
        auto thread = state.process->CreateThread(entryAddr, entryArg, stackTop, priority);
        state.logger->Debug("svcCreateThread: Created thread with handle 0x{:X} (Entry Point: 0x{:X}, Argument: 0x{:X}, Stack Pointer: 0x{:X}, Priority: {}, PID: {})", thread->handle, entryAddr, entryArg, stackTop, priority, thread->pid);
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
            u64 addr = state.ctx->registers.x1;
            if ((addr & (PAGE_SIZE - 1U))) {
                state.ctx->registers.w0 = constant::status::InvAddress;
                state.logger->Warn("svcMapSharedMemory: 'address' not page aligned: 0x{:X}", addr);
                return;
            }
            const u64 size = state.ctx->registers.x2;
            if ((size & (PAGE_SIZE - 1U)) || !size) {
                state.ctx->registers.w0 = constant::status::InvSize;
                state.logger->Warn("svcMapSharedMemory: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
                return;
            }
            u32 perm = state.ctx->registers.w3;
            memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
            if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
                state.logger->Warn("svcMapSharedMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
                state.ctx->registers.w0 = constant::status::InvPermission;
                return;
            }
            state.logger->Debug("svcMapSharedMemory: Mapping shared memory at 0x{:X} for {} bytes ({}{}{})", addr, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            object->Map(addr, size, permission);
            state.ctx->registers.w0 = constant::status::Success;
        } catch (const std::exception &) {
            state.logger->Warn("svcMapSharedMemory: 'handle' invalid: 0x{:X}", state.ctx->registers.w0);
            state.ctx->registers.w0 = constant::status::InvHandle;
        }
    }

    void CreateTransferMemory(DeviceState &state) {
        u64 addr = state.ctx->registers.x1;
        if ((addr & (PAGE_SIZE - 1U))) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcCreateTransferMemory: 'address' not page aligned: {}", addr);
            return;
        }
        u64 size = state.ctx->registers.x2;
        if ((size & (PAGE_SIZE - 1U)) || !size) {
            state.ctx->registers.w0 = constant::status::InvSize;
            state.logger->Warn("svcCreateTransferMemory: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
            return;
        }
        u32 perm = state.ctx->registers.w3;
        memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
        if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
            state.logger->Warn("svcCreateTransferMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            state.ctx->registers.w0 = constant::status::InvPermission;
            return;
        }
        state.logger->Debug("svcCreateTransferMemory: Creating transfer memory at 0x{:X} for {} bytes ({}{}{})", addr, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
        auto shmem = state.process->NewHandle<type::KTransferMemory>(state.process->pid, addr, size, permission);
        state.ctx->registers.w0 = constant::status::Success;
        state.ctx->registers.w1 = shmem.handle;
    }

    void CloseHandle(DeviceState &state) {
        auto handle = static_cast<handle_t>(state.ctx->registers.w0);
        try {
            state.process->handleTable.erase(handle);
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
            auto &object = state.process->handleTable.at(handle);
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
            auto object = state.process->handleTable.at(handle);
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
        auto start = GetCurrTimeNs();
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
            if ((GetCurrTimeNs() - start) >= timeout) {
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
            state.logger->Warn("svcArbitrateLock: 'address' not word aligned: {}", addr);
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
        auto addr = state.ctx->registers.x0;
        if ((addr & ((1UL << WORD_BIT) - 1U))) {
            state.ctx->registers.w0 = constant::status::InvAddress;
            state.logger->Warn("svcArbitrateUnlock: 'address' not word aligned: {}", addr);
            return;
        }
        state.logger->Debug("svcArbitrateUnlock: Unlocking mutex at 0x{:X}", addr);
        state.process->MutexUnlock(addr);
        state.ctx->registers.w0 = constant::status::Success;
    }

    void WaitProcessWideKeyAtomic(DeviceState &state) {
        auto mtxAddr = state.ctx->registers.x0;
        auto condAddr = state.ctx->registers.x1;
        try {
            auto &cvar = state.process->condVarMap.at(condAddr);
            if ((mtxAddr & ((1UL << WORD_BIT) - 1U))) {
                state.ctx->registers.w0 = constant::status::InvAddress;
                state.logger->Warn("svcWaitProcessWideKeyAtomic: mutex address not word aligned: {}", mtxAddr);
                return;
            }
            auto handle = state.ctx->registers.w2;
            if (handle != state.thread->handle)
                throw exception("svcWaitProcessWideKeyAtomic: Called from another thread");
            state.process->MutexLock(mtxAddr);
            auto &mutex = state.process->mutexMap.at(mtxAddr);
            auto timeout = state.ctx->registers.x3;
            state.logger->Debug("svcWaitProcessWideKeyAtomic: Mutex: 0x{:X}, Conditional-Variable: 0x:{:X}, Timeout: {} ns", mtxAddr, condAddr, timeout);
            timespec spec{};
            clock_gettime(CLOCK_REALTIME, &spec);
            u128 time = u128(spec.tv_sec * 1000000000U + spec.tv_nsec) + timeout; // u128 to prevent overflow
            spec.tv_sec = static_cast<time_t>(time / 1000000000U);
            spec.tv_nsec = static_cast<long>(time % 1000000000U);
            if (pthread_cond_timedwait(&cvar, &mutex, &spec) == ETIMEDOUT)
                state.ctx->registers.w0 = constant::status::Timeout;
            else
                state.ctx->registers.w0 = constant::status::Success;
            state.process->MutexUnlock(mtxAddr);
        } catch (const std::out_of_range &) {
            state.logger->Debug("svcWaitProcessWideKeyAtomic: No Conditional-Variable at 0x{:X}", condAddr);
            state.process->condVarMap[condAddr] = PTHREAD_COND_INITIALIZER;
            state.ctx->registers.w0 = constant::status::Success;
        }
    }

    void SignalProcessWideKey(DeviceState &state) {
        auto address = state.ctx->registers.x0;
        auto count = state.ctx->registers.w1;
        try {
            state.logger->Debug("svcSignalProcessWideKey: Signalling Conditional-Variable at 0x{:X} for {}", address, count);
            auto &cvar = state.process->condVarMap.at(address);
            if(count == UINT32_MAX)
                pthread_cond_broadcast(&cvar);
            else
                for (u32 iter = 0; iter < count; iter++)
                    pthread_cond_signal(&cvar);
        } catch (const std::out_of_range &) {
            state.logger->Debug("svcSignalProcessWideKey: No Conditional-Variable at 0x{:X}", address);
            state.process->condVarMap[address] = PTHREAD_COND_INITIALIZER;
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
                out = constant::MapAddr;
                break;
            case constant::infoState::AliasRegionSize:
                out = constant::MapSize;
                break;
            case constant::infoState::HeapRegionBaseAddr:
                out = state.process->memoryRegionMap.at(memory::Region::Heap)->address;
                break;
            case constant::infoState::HeapRegionSize:
                out = state.process->memoryRegionMap.at(memory::Region::Heap)->size;
                break;
            case constant::infoState::TotalMemoryAvailable:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::TotalMemoryUsage:
                out = state.process->memoryRegionMap.at(memory::Region::Heap)->address + state.process->mainThreadStackSz + state.process->GetProgramSize();
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                out = constant::BaseAddr;
                break;
            case constant::infoState::AddressSpaceSize:
                out = constant::BaseEnd;
                break;
            case constant::infoState::StackRegionBaseAddr:
                out = state.thread->stackTop;
                break;
            case constant::infoState::StackRegionSize:
                out = state.process->mainThreadStackSz;
                break;
            case constant::infoState::PersonalMmHeapSize:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::PersonalMmHeapUsage:
                out = state.process->memoryRegionMap.at(memory::Region::Heap)->address + state.process->mainThreadStackSz;
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                out = constant::TotalPhyMem; // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                out = state.process->memoryRegionMap.at(memory::Region::Heap)->address + state.process->mainThreadStackSz; // TODO: Same as above
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
