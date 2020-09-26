// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISession.h"

namespace skyline::service::apm {
    ISession::ISession(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISession::SetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto mode{request.Pop<u32>()};
        auto config{request.Pop<u32>()};
        performanceConfig.at(mode) = config;
        state.logger->Info("Performance configuration set to 0x{:X} ({})", config, mode ? "Docked" : "Handheld");
        return {};
    }

    Result ISession::GetPerformanceConfiguration(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto performanceMode{request.Pop<u32>()};
        response.Push<u32>(performanceConfig.at(performanceMode));
        return {};
    }
}
