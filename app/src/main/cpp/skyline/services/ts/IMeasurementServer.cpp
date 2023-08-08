// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISession.h"
#include "IMeasurementServer.h"

namespace skyline::service::ts {
    IMeasurementServer::IMeasurementServer(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IMeasurementServer::GetTemperature(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u8 location{request.Pop<u8>()};
        response.Push<u32>(!location ? 35 : 20);
        return {};
    }

    Result IMeasurementServer::GetTemperatureMilliC(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        u8 location{request.Pop<u8>()};
        response.Push<u32>(!location ? 35000 : 20000);
        return {};
    }

    Result IMeasurementServer::OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISession), session, response);
        return {};
    }
}
