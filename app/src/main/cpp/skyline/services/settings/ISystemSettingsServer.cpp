// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "ISystemSettingsServer.h"

namespace skyline::service::settings {
    ISystemSettingsServer::ISystemSettingsServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISystemSettingsServer::GetFirmwareVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        request.outputBuf.at(0).as<SysVerTitle>() = {.major=9, .minor=0, .micro=0, .revMajor=4, .revMinor=0, .platform="NX", .verHash="4de65c071fd0869695b7629f75eb97b2551dbf2f", .dispVer="9.0.0", .dispTitle="NintendoSDK Firmware for NX 9.0.0-4.0"};
        return {};
    }
}
