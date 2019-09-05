#include <cstdint>
#include <string>
#include <syslog.h>
#include <utility>
#include "svc.h"

namespace lightSwitch::kernel::svc {
    void SetHeapSize(device_state &state) {
        uint64_t addr = state.this_process->MapPrivate(0, state.nce->GetRegister(regs::w1), {true, true, false}, memory::Region::heap);
        state.nce->SetRegister(regs::w0, constant::status::success);
        state.nce->SetRegister(regs::x1, addr);
    }

    void CreateThread(device_state &state) {
        // TODO: Check if the values supplied by the process are actually valid & Support Core Mask potentially ?
        auto thread = state.this_process->CreateThread(state.nce->GetRegister(regs::x1), state.nce->GetRegister(regs::x2), state.nce->GetRegister(regs::x3), static_cast<uint8_t>(state.nce->GetRegister(regs::w4)));
        state.nce->SetRegister(regs::w0, constant::status::success);
        state.nce->SetRegister(regs::w1, thread->handle);
    }

    void StartThread(device_state &state) {
        auto& object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(regs::w0)));
        if(object->type==type::KObjectType::KThread) {
            std::static_pointer_cast<type::KThread>(object)->Start();
        } else
            throw exception("StartThread was called on a non-KThread object");
    }

    void ExitThread(device_state &state) {
        state.os->KillThread(state.this_thread->pid);
    }

    void GetThreadPriority(device_state &state) {
        auto& object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(regs::w0)));
        if(object->type==type::KObjectType::KThread) {
            state.nce->SetRegister(regs::w0, constant::status::success);
            state.nce->SetRegister(regs::w1, std::static_pointer_cast<type::KThread>(object)->priority);
        } else
            throw exception("GetThreadPriority was called on a non-KThread object");
    }

    void SetThreadPriority(device_state &state) {
        auto& object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(regs::w0)));
        if(object->type==type::KObjectType::KThread) {
            std::static_pointer_cast<type::KThread>(object)->Start();
        } else
            throw exception("SetThreadPriority was called on a non-KThread object");
    }

    void CloseHandle(device_state &state) {
        auto& object = state.this_process->handle_table.at(static_cast<const unsigned int &>(state.nce->GetRegister(regs::w0)));
        switch (object->type) {
            case(type::KObjectType::KThread):
                state.os->KillThread(std::static_pointer_cast<type::KThread>(object)->pid);
                break;
            case(type::KObjectType::KProcess):
                state.os->KillThread(std::static_pointer_cast<type::KProcess>(object)->main_thread);
                break;
        }
        state.nce->SetRegister(regs::w0, constant::status::success);
    }

    void ConnectToNamedPort(device_state &state) {
        char port[constant::port_size]{0};
        state.os->this_process->ReadMemory(port, state.nce->GetRegister(regs::x1), constant::port_size);
        if (std::strcmp(port, "sm:") == 0)
            state.nce->SetRegister(regs::w1, constant::sm_handle);
        else
            throw exception(fmt::format("svcConnectToNamedPort tried connecting to invalid port: \"{}\"", port));
        state.nce->SetRegister(regs::w0, constant::status::success);
    }

    void SendSyncRequest(device_state &state) {
        state.logger->Write(Logger::DEBUG, "svcSendSyncRequest called for handle 0x{:X}.", state.nce->GetRegister(regs::x0));
        uint8_t tls[constant::tls_ipc_size];
        state.os->this_process->ReadMemory(&tls, state.this_thread->tls, constant::tls_ipc_size);
        ipc::IpcRequest request(tls, state);
        state.os->IpcHandler(request);
    }

    void OutputDebugString(device_state &state) {
        std::string debug(state.nce->GetRegister(regs::x1), '\0');
        state.os->this_process->ReadMemory((void *) debug.data(), state.nce->GetRegister(regs::x0), state.nce->GetRegister(regs::x1));
        state.logger->Write(Logger::INFO, "svcOutputDebugString: {}", debug.c_str());
        state.nce->SetRegister(regs::w0, 0);
    }

    void GetInfo(device_state &state) {
        state.logger->Write(Logger::DEBUG, "svcGetInfo called with ID0: {}, ID1: {}", state.nce->GetRegister(regs::w1), state.nce->GetRegister(regs::x3));
        switch (state.nce->GetRegister(regs::w1)) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
            case constant::infoState::TitleId:
            case constant::infoState::PrivilegedProcessId:
                state.nce->SetRegister(regs::x1, 0);
                break;
            case constant::infoState::HeapRegionBaseAddr:
                state.nce->SetRegister(regs::x1, state.os->this_process->memory_map.at(memory::Region::heap).address);
                break;
            case constant::infoState::HeapRegionSize:
                state.nce->SetRegister(regs::x1, state.os->this_process->memory_map.at(memory::Region::heap).size);
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                state.nce->SetRegister(regs::x1, constant::base_addr);
                break;
            case constant::infoState::AddressSpaceSize:
                state.nce->SetRegister(regs::x1, constant::base_size);
                break;
            case constant::infoState::PersonalMmHeapSize:
                state.nce->SetRegister(regs::x1, constant::total_phy_mem);
                break;
            case constant::infoState::PersonalMmHeapUsage:
                state.nce->SetRegister(regs::x1, state.os->this_process->memory_map.at(memory::Region::heap).address + state.this_process->main_thread_stack_sz);
                break;
            case constant::infoState::TotalMemoryAvailableWithoutMmHeap:
                state.nce->SetRegister(regs::x1, constant::total_phy_mem); // TODO: NPDM specifies SystemResourceSize, subtract that from this
                break;
            case constant::infoState::TotalMemoryUsedWithoutMmHeap:
                state.nce->SetRegister(regs::x1, state.os->this_process->memory_map.at(memory::Region::heap).address + state.this_process->main_thread_stack_sz); // TODO: Same as above
                break;
            case constant::infoState::UserExceptionContextAddr:
                state.nce->SetRegister(regs::x1, state.this_process->tls_pages[0]->Get(0));
                break;
            default:
                state.logger->Write(Logger::WARN, "Unimplemented svcGetInfo with ID0: {}, ID1: {}", state.nce->GetRegister(regs::w1), state.nce->GetRegister(regs::x3));
                state.nce->SetRegister(regs::w0, constant::status::unimpl);
                return;
        }
        state.nce->SetRegister(regs::w0, constant::status::success);
    }

    void ExitProcess(device_state &state) {
        state.os->KillThread(state.this_process->main_thread);
    }
}
