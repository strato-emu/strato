#include <kernel/types/KProcess.h>
#include "ISystemSettingsServer.h"

namespace skyline::service::settings {
    ISystemSettingsServer::ISystemSettingsServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, Service::settings_ISystemSettingsServer, "settings:ISystemSettingsServer", {
        {0x3, SFUNC(ISystemSettingsServer::GetFirmwareVersion)}}) {}

    void ISystemSettingsServer::GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        SysVerTitle title{.minor=9, .major=0, .micro=0, .revMajor=4, .platform="NX", .verHash="4de65c071fd0869695b7629f75eb97b2551dbf2f", .dispVer="9.0.0", .dispTitle="NintendoSDK Firmware for NX 9.0.0-4.0"};
        state.process->WriteMemory(title, request.outputBuf.at(0).address);
    }
}
