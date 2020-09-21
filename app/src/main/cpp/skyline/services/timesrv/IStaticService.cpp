// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISteadyClock.h"
#include "ISystemClock.h"
#include "ITimeZoneService.h"
#include "IStaticService.h"

namespace skyline::service::timesrv {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IStaticService::GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::User, state, manager), session, response);
        return {};
    }

    Result IStaticService::GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Network, state, manager), session, response);
        return {};
    }

    Result IStaticService::GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISteadyClock>(state, manager), session, response);
        return {};
    }

    Result IStaticService::GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ITimeZoneService>(state, manager), session, response);
        return {};
    }

    Result IStaticService::GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Local, state, manager), session, response);
        return {};
    }
}
