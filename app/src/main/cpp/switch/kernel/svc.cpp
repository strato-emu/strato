#include <cstdint>
#include <string>
#include <syslog.h>
#include <utility>
#include "svc.h"

namespace lightSwitch::kernel::svc {
    void SetHeapSize(device_state &state) {
        auto heap = state.this_process->MapPrivateRegion(0, state.nce->GetRegister(wreg::w1), {true, true, false}, Memory::Type::Heap, Memory::Region::heap);
        state.nce->SetRegister(wreg::w0, constant::status::success);
        state.nce->SetRegister(xreg::x1, heap->address);
        state.logger->Write(Logger::DEBUG, "Heap size was set to 0x{:X}", state.nce->GetRegister(wreg::w1));
    }

    void QueryMemory(device_state &state) {
        Memory::MemoryInfo mem_inf;
        u64 addr = state.nce->GetRegister(xreg::x2);
        if (state.nce->memory_map.count(addr)) {
            mem_inf = state.nce->memory_map.at(addr)->GetInfo(state.this_process->main_thread);
        } else if (state.this_process->memory_map.count(addr)) {
            mem_inf = state.this_process->memory_map.at(addr)->GetInfo();
        } else {
            state.nce->SetRegister(wreg::w0, constant::status::inv_address);
            return;
        }
        state.this_process->WriteMemory<Memory::MemoryInfo>(mem_inf, state.nce->GetRegister(xreg::x0));
        state.nce->SetRegister(wreg::w0, constant::status::success);
    }

    void CreateThread(device_state &state) {
        // TODO: Check if the values supplied by the process are actually valid & Support Core Mask potentially ?
        auto thread = state.this_process->CreateThread(state.nce->GetRegister(xreg::x1), state.nce->GetRegister(xreg::x2), state.nce->GetRegister(xreg::x3), static_cast<u8>(state.nce->GetRegister(wreg::w4)));
        state.nce->SetRegister(wreg::w0, constant::status::success);
        state.nce->SetRegister(wreg::w1, thread->handle);
    }

    void StartThread(device_state &state) {
        auto &object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(wreg::w0)));
        if (object->type == type::KObjectType::KThread) {
            std::static_pointer_cast<type::KThread>(object)->Start();
        } else
            throw exception("StartThread was called on a non-KThread object");
    }

    void ExitThread(device_state &state) {
        state.os->KillThread(state.this_thread->pid);
    }

    void GetThreadPriority(device_state &state) {
        auto &object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(wreg::w0)));
        if (object->type == type::KObjectType::KThread) {
            state.nce->SetRegister(wreg::w0, constant::status::success);
            state.nce->SetRegister(wreg::w1, std::static_pointer_cast<type::KThread>(object)->priority);
        } else
            throw exception("GetThreadPriority was called on a non-KThread object");
    }

    void SetThreadPriority(device_state &state) {
        auto &object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(wreg::w0)));
        if (object->type == type::KObjectType::KThread) {
            std::static_pointer_cast<type::KThread>(object)->Start();
        } else
            throw exception("SetThreadPriority was called on a non-KThread object");
    }

    void CloseHandle(device_state &state) {
        auto &object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(wreg::w0)));
        switch (object->type) {
            case (type::KObjectType::KThread):
                state.os->KillThread(std::static_pointer_cast<type::KThread>(object)->pid);
                break;
            case (type::KObjectType::KProcess):
                state.os->KillThread(std::static_pointer_cast<type::KProcess>(object)->main_thread);
                break;
            default:
                state.nce->SetRegister(wreg::w0, constant::status::inv_handle);
                return;
        }
        state.nce->SetRegister(wreg::w0, constant::status::success);
    }

    void ConnectToNamedPort(device_state &state) {
        char port[constant::port_size]{0};
        state.os->this_process->ReadMemory(port, state.nce->GetRegister(xreg::x1), constant::port_size);
        if (std::strcmp(port, "sm:") == 0)
            state.nce->SetRegister(wreg::w1, constant::sm_handle);
        else
            throw exception(fmt::format("svcConnectToNamedPort tried connecting to invalid port: \"{}\"", port));
        state.nce->SetRegister(wreg::w0, constant::status::success);
    }

    void SendSyncRequest(device_state &state) {
        state.logger->Write(Logger::DEBUG, "----------------------------svcSendSyncRequest Start-----------------------");
        state.logger->Write(Logger::DEBUG, "svcSendSyncRequest called for handle 0x{:X}.", state.nce->GetRegister(xreg::x0));
        state.os->IpcHandler(static_cast<handle_t>(state.nce->GetRegister(xreg::x0)));
        state.nce->SetRegister(wreg::w0, constant::status::success);
        state.nce->SetRegister(wreg::w19, constant::status::success);
        state.logger->Write(Logger::DEBUG, "----------------------------svcSendSyncRequest End-------------------------");
    }

    void OutputDebugString(device_state &state) {
        std::string debug(state.nce->GetRegister(xreg::x1), '\0');
        state.os->this_process->ReadMemory((void *) debug.data(), state.nce->GetRegister(xreg::x0), state.nce->GetRegister(xreg::x1));
        state.logger->Write(Logger::INFO, "svcOutputDebugString: {}", debug.c_str());
        state.nce->SetRegister(wreg::w0, 0);
    }

    void GetInfo(device_state &state) {
        state.logger->Write(Logger::DEBUG, "svcGetInfo called with ID0: {}, ID1: {}", state.nce->GetRegister(wreg::w1), state.nce->GetRegister(xreg::x3));
        switch (state.nce->GetRegister(wreg::w1)) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
            case constant::infoState::TitleId:
            case constant::infoState::PrivilegedProcessId:
                state.nce->SetRegister(xreg::x1, 0);
                break;
            case constant::infoState::AliasRegionBaseAddr:
                state.nce->SetRegister(xreg::x1, constant::map_addr);
                break;
            case constant::infoState::AliasRegionSize:
                state.nce->SetRegister(xreg::x1, constant::map_size);
                break;
            case constant::infoState::HeapRegionBaseAddr:
                state.nce->SetRegister(xreg::x1, state.os->this_process->memory_region_map.at(Memory::Region::heap)->address);
                break;
            case constant::infoState::HeapRegionSize:
                state.nce->SetRegister(xreg::x1, state.os->this_process->memory_region_map.at(Memory::Region::heap)->size);
                break;
            case constant::infoState::TotalMemoryAvailable:
                state.nce->SetRegister(xreg::x1, constant::total_phy_mem);
                break;
            case constant::infoState::TotalMemoryUsage:
                state.nce->SetRegister(xreg::x1, state.os->this_process->memory_region_map.at(Memory::Region::heap)->address + state.this_process->main_thread_stack_sz + state.nce->GetSharedSize());
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                state.nce->SetRegister(xreg::x1, constant::base_addr);
                break;
            case constant::infoState::AddressSpaceSize:
                state.nce->SetRegister(xreg::x1, constant::base_size);
                break;
            case constant::infoState::StackRegionBaseAddr:
                state.nce->SetRegister(xreg::x1, state.this_thread->stack_top);
                break;
            case constant::infoState::StackRegionSize:
                state.nce->SetRegister(xreg::x1, state.this_process->main_thread_stack_sz);
                break;
            case constant::infoState::PersonalMmHeapSize:
                state.nce->SetRegister(xreg::x1, constant::total_phy_mem);
                break;
            case constant::infoState::PersonalMmHeapUsage:
                state.nce->SetRegister(xreg::x1, state.os->this_process->memory_region_map.at(Memory::Region::heap)->address + state.this_process->main_thread_stack_sz);
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                state.nce->SetRegister(xreg::x1, constant::total_phy_mem); // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                state.nce->SetRegister(xreg::x1, state.os->this_process->memory_region_map.at(Memory::Region::heap)->address + state.this_process->main_thread_stack_sz); // TODO: Same as above
                break;
            case constant::infoState::UserExceptionContextAddr:
                state.nce->SetRegister(xreg::x1, state.this_process->tls_pages[0]->Get(0));
                break;
            default:
                state.logger->Write(Logger::WARN, "Unimplemented svcGetInfo with ID0: {}, ID1: {}", state.nce->GetRegister(wreg::w1), state.nce->GetRegister(xreg::x3));
                state.nce->SetRegister(wreg::w0, constant::status::unimpl);
                return;
        }
        state.nce->SetRegister(wreg::w0, constant::status::success);
    }

    void ExitProcess(device_state &state) {
        state.os->KillThread(state.this_process->main_thread);
    }
}
