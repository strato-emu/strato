// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISteadyClock.h"
#include "ISystemClock.h"
#include "ITimeZoneService.h"
#include "IStaticService.h"

namespace skyline::service::timesrv {
    IStaticService::IStaticService(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager, {
        {0x0, SFUNC(IStaticService::GetStandardUserSystemClock)},
        {0x1, SFUNC(IStaticService::GetStandardNetworkSystemClock)},
        {0x2, SFUNC(IStaticService::GetStandardSteadyClock)},
        {0x3, SFUNC(IStaticService::GetTimeZoneService)},
        {0x4, SFUNC(IStaticService::GetStandardLocalSystemClock)}
    }) {}

    void IStaticService::GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::User, state, manager), session, response);
    }

    void IStaticService::GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Network, state, manager), session, response);
    }

    void IStaticService::GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISteadyClock>(state, manager), session, response);
    }

    void IStaticService::GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ITimeZoneService>(state, manager), session, response);
    }

    void IStaticService::GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<ISystemClock>(SystemClockType::Local, state, manager), session, response);
    }
}
