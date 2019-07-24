#include <cstdint>
#include <string>
#include <syslog.h>
#include <utility>
#include <unicorn/arm64.h>
#include "svc.h"

namespace lightSwitch::os::svc {
    void ConnectToNamedPort(device_state &state) {
        char port[constant::port_size]{0};
        state.mem->Read(port, state.cpu->GetRegister(UC_ARM64_REG_X1), constant::port_size);
        if (std::strcmp(port, "sm:") == 0)
            state.cpu->SetRegister(UC_ARM64_REG_W1, constant::sm_handle);
        else {
            state.logger->write(Logger::ERROR, "svcConnectToNamedPort tried connecting to invalid port \"{0}\"", port);
            state.cpu->StopExecution();
        }
        state.cpu->SetRegister(UC_ARM64_REG_W0, 0);
    }

    void SendSyncRequest(device_state &state) {
        state.logger->write(Logger::DEBUG, "svcSendSyncRequest called for handle 0x{0:x}.", state.cpu->GetRegister(UC_ARM64_REG_X0));
        uint8_t tls[constant::tls_ipc_size];
        state.mem->Read(&tls, constant::tls_addr, constant::tls_ipc_size);
        ipc::IpcRequest request(tls, state);
        state.cpu->SetRegister(UC_ARM64_REG_W0, 0);
    }

    void OutputDebugString(device_state &state) {
        std::string debug(state.cpu->GetRegister(UC_ARM64_REG_X1), '\0');
        state.mem->Read((void *) debug.data(), state.cpu->GetRegister(UC_ARM64_REG_X0), state.cpu->GetRegister(UC_ARM64_REG_X1));
        state.logger->write(Logger::INFO, "ROM Output: {0}", debug.c_str());
        state.cpu->SetRegister(UC_ARM64_REG_W0, 0);
    }

    void GetInfo(device_state &state) {
        switch (state.cpu->GetRegister(UC_ARM64_REG_X1)) {
            case constant::infoState::AllowedCpuIdBitmask:
            case constant::infoState::AllowedThreadPriorityMask:
            case constant::infoState::IsCurrentProcessBeingDebugged:
                state.cpu->SetRegister(UC_ARM64_REG_X1, 0);
                break;
            case constant::infoState::AddressSpaceBaseAddr:
                state.cpu->SetRegister(UC_ARM64_REG_X1, constant::base_addr);
                break;
            case constant::infoState::TitleId:
                state.cpu->SetRegister(UC_ARM64_REG_X1, 0); // TODO: Complete this
                break;
            default:
                state.logger->write(Logger::WARN, "Unimplemented GetInfo call. ID1: {0}, ID2: {1}", state.cpu->GetRegister(UC_ARM64_REG_X1), state.cpu->GetRegister(UC_ARM64_REG_X3));
                state.cpu->SetRegister(UC_ARM64_REG_X1, constant::svc_unimpl);
                return;
        }
        state.cpu->SetRegister(UC_ARM64_REG_W0, 0);
    }

    void ExitProcess(device_state &state) {
        state.cpu->StopExecution();
    }
}