#include "svc.h"
#include <os.h>

namespace skyline::kernel::svc {
    void SetHeapSize(DeviceState &state) {
        const u32 size = state.nce->GetRegister(Wreg::W1);
        if(size%constant::HeapSizeDiv != 0) {
            state.nce->SetRegister(Xreg::X1, 0);
            state.nce->SetRegister(Wreg::W0, constant::status::InvSize);
            state.logger->Warn("svcSetHeapSize: 'size' not divisible by 2MB: {}", size);
            return;
        }
        std::shared_ptr<type::KPrivateMemory> heap;
        try {
            heap = state.thisProcess->memoryRegionMap.at(memory::Region::Heap);
            heap->Resize(size, true);
        } catch (const exception &) {
            state.logger->Warn("svcSetHeapSize: Falling back to recreating memory");
            state.thisProcess->UnmapPrivateRegion(memory::Region::Heap);
            heap = state.thisProcess->MapPrivateRegion(constant::HeapAddr, size, {true, true, false}, memory::Type::Heap, memory::Region::Heap).item;
        }
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Xreg::X1, heap->address);
        state.logger->Debug("svcSetHeapSize: Allocated at 0x{:X} for 0x{:X} bytes", heap->address, heap->size);
    }

    void SetMemoryAttribute(DeviceState &state) {
        const u64 addr = state.nce->GetRegister(Xreg::X0);
        if((addr & (PAGE_SIZE - 1))) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcSetMemoryAttribute: 'address' not page aligned: {}", addr);
            return;
        }
        const u64 size = state.nce->GetRegister(Xreg::X1);
        if((size & (PAGE_SIZE - 1)) || !size) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvSize);
            state.logger->Warn("svcSetMemoryAttribute: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
            return;
        }
        u32 mask = state.nce->GetRegister(Wreg::W2);
        u32 value = state.nce->GetRegister(Wreg::W3);
        u32 maskedValue = mask | value;
        if(maskedValue != mask) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvCombination);
            state.logger->Warn("svcSetMemoryAttribute: 'mask' invalid: 0x{:X}, 0x{:X}", mask, value);
            return;
        }
        memory::MemoryAttribute attribute = *reinterpret_cast<memory::MemoryAttribute*>(&maskedValue);
        bool found = false;
        for (const auto&[address, region] : state.thisProcess->memoryMap) {
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
        if(!found) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcSetMemoryAttribute: Cannot find memory region: 0x{:X}", addr);
            return;
        }
        state.logger->Debug("svcSetMemoryAttribute: Set caching to {} at 0x{:X} for 0x{:X} bytes", !attribute.isUncached, addr, size);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void QueryMemory(DeviceState &state) {
        memory::MemoryInfo memInf;
        u64 addr = state.nce->GetRegister(Xreg::X2);
        bool found = false;
        for (const auto&[address, region] : state.thisProcess->memoryMap) {
            if (addr >= address && addr < (address + region->size)) {
                memInf = region->GetInfo(addr);
                found = true;
                break;
            }
        }
        if (!found) {
            for (const auto &object : state.thisProcess->handleTable) {
                if (object.second->objectType == type::KType::KSharedMemory) {
                    const auto &mem = state.thisProcess->GetHandle<type::KSharedMemory>(object.first);
                    if (mem->procInfMap.count(state.thisProcess->mainThread)) {
                        const auto &map = mem->procInfMap.at(state.thisProcess->mainThread);
                        if (addr >= map.address && addr < (map.address + map.size)) {
                            memInf = mem->GetInfo(state.thisProcess->mainThread);
                            found = true;
                            break;
                        }
                    }
                } else if (object.second->objectType == type::KType::KTransferMemory) {
                    const auto &mem = state.thisProcess->GetHandle<type::KTransferMemory>(object.first);
                    if (addr >= mem->cAddress && addr < (mem->cAddress + mem->cSize)) {
                        memInf = mem->GetInfo();
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                memInf = {
                    .baseAddress = constant::BaseEnd,
                    .size = static_cast<u64>(-constant::BaseEnd + 1),
                    .type = static_cast<u64>(memory::Type::Unmapped)
                };
                state.logger->Debug("svcQueryMemory: Cannot find block of address: 0x{:X}", addr);
            }
        }
        state.logger->Debug("svcQueryMemory: Address: 0x{:X}, Size: 0x{:X}, Type: {}, Is Uncached: {}, Permissions: {}{}{}", memInf.baseAddress, memInf.size, memInf.type, static_cast<bool>(memInf.memoryAttribute.isUncached), memInf.perms.r ? "R" : "-", memInf.perms.w ? "W" : "-", memInf.perms.x ? "X" : "-");
        state.thisProcess->WriteMemory<memory::MemoryInfo>(memInf, state.nce->GetRegister(Xreg::X0));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void ExitProcess(DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting current process: {}", state.thisProcess->mainThread);
        state.os->KillThread(state.thisProcess->mainThread);
    }

    void CreateThread(DeviceState &state) {
        u64 entryAddr = state.nce->GetRegister(Xreg::X1);
        u64 entryArg = state.nce->GetRegister(Xreg::X2);
        u64 stackTop = state.nce->GetRegister(Xreg::X3);
        u8 priority = static_cast<u8>(state.nce->GetRegister(Wreg::W4));
        if(priority >= constant::PriorityNin.first && priority <= constant::PriorityNin.second) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcSetHeapSize: 'priority' invalid: {}", priority);
            return;
        }
        auto thread = state.thisProcess->CreateThread(entryAddr, entryArg, stackTop, priority);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Wreg::W1, thread->handle);
        state.logger->Info("svcCreateThread: Created thread with handle 0x{:X}", thread->handle);
    }

    void StartThread(DeviceState &state) {
        auto handle = state.nce->GetRegister(Wreg::W0);
        try {
            auto thread = state.thisProcess->GetHandle<type::KThread>(handle);
            state.logger->Debug("svcStartThread: Starting thread: 0x{:X}, {}", handle, thread->pid);
            thread->Start();
        } catch (const std::exception&) {
            state.logger->Warn("svcStartThread: 'handle' invalid: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
        }
    }

    void ExitThread(DeviceState &state) {
        state.logger->Debug("svcExitProcess: Exiting current thread: {}", state.thisThread->pid);
        state.os->KillThread(state.thisThread->pid);
    }

    void SleepThread(DeviceState &state) {
        auto in = state.nce->GetRegister(Xreg::X0);
        switch (in) {
            case 0:
            case 1:
            case 2:
                state.logger->Debug("svcSleepThread: Yielding thread: {}", in);
                state.thisThread->status = type::KThread::Status::Runnable; // Will cause the application to awaken on the next iteration of the main loop
                break;
            default:
                state.logger->Debug("svcSleepThread: Thread sleeping for {} ns", in);
                state.thisThread->timeout = GetCurrTimeNs() + in;
                state.thisThread->status = type::KThread::Status::Sleeping;
        }
    }

    void GetThreadPriority(DeviceState &state) {
        auto handle = state.nce->GetRegister(Wreg::W0);
        try {
            auto priority = state.thisProcess->GetHandle<type::KThread>(handle)->priority;
            state.nce->SetRegister(Wreg::W1, priority);
            state.nce->SetRegister(Wreg::W0, constant::status::Success);
            state.logger->Debug("svcGetThreadPriority: Writing thread priority {}", priority);
        } catch (const std::exception&) {
            state.logger->Warn("svcGetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
        }
    }

    void SetThreadPriority(DeviceState &state) {
        auto handle = state.nce->GetRegister(Wreg::W0);
        auto priority = state.nce->GetRegister(Wreg::W1);
        try {
            state.thisProcess->GetHandle<type::KThread>(handle)->UpdatePriority(static_cast<u8>(priority));
            state.nce->SetRegister(Wreg::W0, constant::status::Success);
            state.logger->Debug("svcSetThreadPriority: Setting thread priority to {}", priority);
        } catch (const std::exception&) {
            state.logger->Warn("svcSetThreadPriority: 'handle' invalid: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
        }
    }

    void MapSharedMemory(DeviceState &state) {
        try {
            auto object = state.thisProcess->GetHandle<type::KSharedMemory>(state.nce->GetRegister(Wreg::W0));
            u64 addr = state.nce->GetRegister(Xreg::X1);
            if ((addr & (PAGE_SIZE - 1U))) {
                state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
                state.logger->Warn("svcMapSharedMemory: 'address' not page aligned: 0x{:X}", addr);
                return;
            }
            const u64 size = state.nce->GetRegister(Xreg::X2);
            if ((size & (PAGE_SIZE - 1U)) || !size) {
                state.nce->SetRegister(Wreg::W0, constant::status::InvSize);
                state.logger->Warn("svcMapSharedMemory: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
                return;
            }
            u32 perm = state.nce->GetRegister(Wreg::W3);
            memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
            if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
                state.logger->Warn("svcMapSharedMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
                state.nce->SetRegister(Wreg::W0, constant::status::InvPermission);
                return;
            }
            state.logger->Debug("svcMapSharedMemory: Mapping shared memory at 0x{:X} for {} bytes ({}{}{})", addr, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            object->Map(addr, size, permission, state.thisProcess->mainThread);
            state.nce->SetRegister(Wreg::W0, constant::status::Success);
        } catch (const std::exception &) {
            state.logger->Warn("svcMapSharedMemory: 'handle' invalid: 0x{:X}", state.nce->GetRegister(Wreg::W0));
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
        }
    }

    void CreateTransferMemory(DeviceState &state) {
        u64 addr = state.nce->GetRegister(Xreg::X1);
        if ((addr & (PAGE_SIZE - 1U))) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcCreateTransferMemory: 'address' not page aligned: {}", addr);
            return;
        }
        u64 size = state.nce->GetRegister(Xreg::X2);
        if ((size & (PAGE_SIZE - 1U)) || !size) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvSize);
            state.logger->Warn("svcCreateTransferMemory: 'size' {}: {}", size ? "not page aligned" : "is zero", size);
            return;
        }
        u32 perm = state.nce->GetRegister(Wreg::W3);
        memory::Permission permission = *reinterpret_cast<memory::Permission *>(&perm);
        if ((permission.w && !permission.r) || (permission.x && !permission.r)) {
            state.logger->Warn("svcCreateTransferMemory: 'permission' invalid: {}{}{}", permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
            state.nce->SetRegister(Wreg::W0, constant::status::InvPermission);
            return;
        }
        state.logger->Debug("svcCreateTransferMemory: Creating transfer memory at 0x{:X} for {} bytes ({}{}{})", addr, size, permission.r ? "R" : "-", permission.w ? "W" : "-", permission.x ? "X" : "-");
        auto shmem = state.thisProcess->NewHandle<type::KTransferMemory>(state.thisProcess->mainThread, addr, size, permission);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        state.nce->SetRegister(Wreg::W1, shmem.handle);
    }

    void CloseHandle(DeviceState &state) {
        auto handle = static_cast<handle_t>(state.nce->GetRegister(Wreg::W0));
        try {
            auto &object = state.thisProcess->handleTable.at(handle);
            switch (object->objectType) {
                case (type::KType::KThread):
                    state.os->KillThread(std::static_pointer_cast<type::KThread>(object)->pid);
                    break;
                case (type::KType::KProcess):
                    state.os->KillThread(std::static_pointer_cast<type::KProcess>(object)->mainThread);
                    break;
                default:
                    state.thisProcess->handleTable.erase(handle);
            }
            state.logger->Debug("svcCloseHandle: Closing handle: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::Success);
        } catch(const std::exception&) {
            state.logger->Warn("svcCloseHandle: 'handle' invalid: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
        }
    }

    void ResetSignal(DeviceState &state) {
        auto handle = state.nce->GetRegister(Wreg::W0);
        try {
            auto &object = state.thisProcess->handleTable.at(handle);
            switch (object->objectType) {
                case (type::KType::KEvent):
                    std::static_pointer_cast<type::KEvent>(object)->ResetSignal();
                    break;
                case (type::KType::KProcess):
                    std::static_pointer_cast<type::KProcess>(object)->ResetSignal();
                    break;
                default: {
                    state.logger->Warn("svcResetSignal: 'handle' type invalid: 0x{:X} ({})", handle, object->objectType);
                    state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
                    return;
                }
            }
            state.logger->Debug("svcResetSignal: Resetting signal: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::Success);
        } catch(const std::out_of_range&) {
            state.logger->Warn("svcResetSignal: 'handle' invalid: 0x{:X}", handle);
            state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
            return;
        }
    }

    void WaitSynchronization(DeviceState &state) {
        auto numHandles = state.nce->GetRegister(Wreg::W2);
        if (numHandles > constant::MaxSyncHandles) {
            state.nce->SetRegister(Wreg::W0, constant::status::MaxHandles);
            return;
        }
        std::vector<handle_t> waitHandles(numHandles);
        state.thisProcess->ReadMemory(waitHandles.data(), state.nce->GetRegister(Xreg::X1), numHandles * sizeof(handle_t));
        std::string handleStr;
        uint index{};
        for (const auto &handle : waitHandles) {
            handleStr += fmt::format("* 0x{:X}\n", handle);
            auto object = state.thisProcess->handleTable.at(handle);
            switch (object->objectType) {
                case type::KType::KProcess:
                case type::KType::KThread:
                case type::KType::KEvent:
                case type::KType::KSession:
                    break;
                default: {
                    state.nce->SetRegister(Wreg::W0, constant::status::InvHandle);
                    state.thisThread->ClearWaitObjects();
                    return;
                }
            }
            auto syncObject = std::static_pointer_cast<type::KSyncObject>(object);
            if (syncObject->signalled) {
                state.logger->Debug("svcWaitSynchronization: Signalled handle: 0x{:X}", handle);
                state.nce->SetRegister(Wreg::W0, constant::status::Success);
                state.nce->SetRegister(Wreg::W1, index);
                state.thisThread->ClearWaitObjects();
                return;
            }
            state.thisThread->waitObjects.push_back(syncObject);
            syncObject->waitThreads.emplace_back(state.thisThread->pid, index);
        }
        auto timeout = state.nce->GetRegister(Xreg::X3);
        state.logger->Debug("svcWaitSynchronization: Waiting on handles:\n{}Timeout: 0x{:X} ns", handleStr, timeout);
        if (state.nce->GetRegister(Xreg::X3) != std::numeric_limits<u64>::max())
            state.thisThread->timeout = GetCurrTimeNs() + timeout;
        else
            state.thisThread->timeout = 0;
        state.thisThread->status = type::KThread::Status::WaitSync;
    }

    void ArbitrateLock(DeviceState &state) {
        auto addr = state.nce->GetRegister(Xreg::X1);
        if((addr & ((1UL << WORD_BIT) - 1U))) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcArbitrateLock: 'address' not word aligned: {}", addr);
            return;
        }
        auto handle = state.nce->GetRegister(Wreg::W2);
        if (handle != state.thisThread->handle)
            throw exception("svcArbitrateLock: Called from another thread");
        state.logger->Debug("svcArbitrateLock: Locking mutex at 0x{:X} for thread 0x{:X}", addr, handle);
        state.thisProcess->MutexLock(addr);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void ArbitrateUnlock(DeviceState &state) {
        auto addr = state.nce->GetRegister(Xreg::X0);
        if((addr & ((1UL << WORD_BIT) - 1U))) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcArbitrateUnlock: 'address' not word aligned: {}", addr);
            return;
        }
        state.logger->Debug("svcArbitrateUnlock: Unlocking mutex at 0x{:X}", addr);
        state.thisProcess->MutexUnlock(addr);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void WaitProcessWideKeyAtomic(DeviceState &state) {
        auto mtxAddr = state.nce->GetRegister(Xreg::X0);
        if((mtxAddr & ((1UL << WORD_BIT) - 1U))) {
            state.nce->SetRegister(Wreg::W0, constant::status::InvAddress);
            state.logger->Warn("svcWaitProcessWideKeyAtomic: mutex address not word aligned: {}", mtxAddr);
            return;
        }
        auto handle = state.nce->GetRegister(Wreg::W2);
        if (handle != state.thisThread->handle)
            throw exception("svcWaitProcessWideKeyAtomic: Called from another thread");
        state.thisProcess->MutexUnlock(mtxAddr);
        auto condAddr = state.nce->GetRegister(Xreg::X1);
        auto &cvarVec = state.thisProcess->condVarMap[condAddr];
        for (auto thread = cvarVec.begin();; thread++) {
            if ((*thread)->priority < state.thisThread->priority) {
                cvarVec.insert(thread, state.thisThread);
                break;
            } else if (thread + 1 == cvarVec.end()) {
                cvarVec.push_back(state.thisThread);
                break;
            }
        }
        auto timeout = state.nce->GetRegister(Xreg::X3);
        state.logger->Debug("svcWaitProcessWideKeyAtomic: Mutex: 0x{:X}, Conditional-Variable: 0x:{:X}, Timeout: {} ns", mtxAddr, condAddr, timeout);
        state.thisThread->status = type::KThread::Status::WaitCondVar;
        state.thisThread->timeout = GetCurrTimeNs() + timeout;
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SignalProcessWideKey(DeviceState &state) {
        auto address = state.nce->GetRegister(Xreg::X0);
        auto count = state.nce->GetRegister(Wreg::W1);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
        if (!state.thisProcess->condVarMap.count(address)) {
            state.logger->Warn("svcSignalProcessWideKey: No Conditional-Variable at 0x{:X}", address);
            return;
        }
        auto &cvarVec = state.thisProcess->condVarMap.at(address);
        count = std::min(count, static_cast<u32>(cvarVec.size()));
        for (uint index = 0; index < count; index++)
            cvarVec[index]->status = type::KThread::Status::Runnable;
        cvarVec.erase(cvarVec.begin(), cvarVec.begin() + count);
        if (cvarVec.empty())
            state.thisProcess->condVarMap.erase(address);
        state.logger->Debug("svcSignalProcessWideKey: Signalling Conditional-Variable at 0x{:X} for {}", address, count);
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
        state.nce->SetRegister(Xreg::X0, tick);
    }

    void ConnectToNamedPort(DeviceState &state) {
        char port[constant::PortSize + 1]{0};
        state.os->thisProcess->ReadMemory(port, state.nce->GetRegister(Xreg::X1), constant::PortSize);
        handle_t handle{};
        if (std::strcmp(port, "sm:") == 0)
            handle = state.os->serviceManager.NewSession(service::Service::sm);
        else {
            state.logger->Warn("svcConnectToNamedPort: Connecting to invalid port: '{}'", port);
            state.nce->SetRegister(Wreg::W0, constant::status::NotFound);
            return;
        }
        state.logger->Debug("svcConnectToNamedPort: Connecting to port '{}' at 0x{:X}", port, handle);
        state.nce->SetRegister(Wreg::W1, handle);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void SendSyncRequest(DeviceState &state) {
        state.os->serviceManager.SyncRequestHandler(static_cast<handle_t>(state.nce->GetRegister(Xreg::X0)));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void GetThreadId(DeviceState &state) {
        pid_t pid{};
        auto handle = state.nce->GetRegister(Wreg::W1);
        if (handle != constant::ThreadSelf) {
            pid = state.thisProcess->GetHandle<type::KThread>(handle)->pid;
        } else
            pid = state.thisThread->pid;
        state.logger->Debug("svcGetThreadId: Handle: 0x{:X}, PID: {}", handle, pid);
        state.nce->SetRegister(Xreg::X1, static_cast<u64>(pid));
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void OutputDebugString(DeviceState &state) {
        std::string debug(state.nce->GetRegister(Xreg::X1), '\0');
        state.os->thisProcess->ReadMemory(debug.data(), state.nce->GetRegister(Xreg::X0), state.nce->GetRegister(Xreg::X1));
        state.logger->Info("Debug Output: {}", debug);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }

    void GetInfo(DeviceState &state) {
        auto id0 = state.nce->GetRegister(Wreg::W1);
        auto handle = state.nce->GetRegister(Wreg::W2);
        auto id1 = state.nce->GetRegister(Xreg::X3);
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
                out = state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address;
                break;
            case constant::infoState::HeapRegionSize:
                out = state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->size;
                break;
            case constant::infoState::TotalMemoryAvailable:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::TotalMemoryUsage:
                out = state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz + state.thisProcess->GetProgramSize();
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                out = constant::BaseAddr;
                break;
            case constant::infoState::AddressSpaceSize:
                out = constant::BaseSize;
                break;
            case constant::infoState::StackRegionBaseAddr:
                out = state.thisThread->stackTop;
                break;
            case constant::infoState::StackRegionSize:
                out = state.thisProcess->mainThreadStackSz;
                break;
            case constant::infoState::PersonalMmHeapSize:
                out = constant::TotalPhyMem;
                break;
            case constant::infoState::PersonalMmHeapUsage:
                out = state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz;
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                out = constant::TotalPhyMem; // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                out = state.os->thisProcess->memoryRegionMap.at(memory::Region::Heap)->address + state.thisProcess->mainThreadStackSz; // TODO: Same as above
                break;
            case constant::infoState::UserExceptionContextAddr:
                out = state.thisProcess->tlsPages[0]->Get(0);
                break;
            default:
                state.logger->Warn("svcGetInfo: Unimplemented case ID0: {}, ID1: {}", id0, id1);
                state.nce->SetRegister(Wreg::W0, constant::status::Unimpl);
                return;
        }
        state.logger->Debug("svcGetInfo: ID0: {}, ID1: {}, Out: 0x{:X}", id0, id1, out);
        state.nce->SetRegister(Xreg::X1, out);
        state.nce->SetRegister(Wreg::W0, constant::status::Success);
    }
}
