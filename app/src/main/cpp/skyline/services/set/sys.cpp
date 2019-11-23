#include "sys.h"
#include <kernel/types/KProcess.h>

namespace skyline::service::set {
    sys::sys(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, false, Service::set_sys, {{0x3, SFUNC(sys::GetFirmwareVersion)}}) {}

    void sys::GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        SysVerTitle title{.minor=9, .major=0, .micro=0, .revMajor=4, .platform="NX", .verHash="4de65c071fd0869695b7629f75eb97b2551dbf2f", .dispVer="9.0.0", .dispTitle="NintendoSDK Firmware for NX 9.0.0-4.0"};
        state.thisProcess->WriteMemory(title, request.outputBuf.at(0).address);
    }
}
