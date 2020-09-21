// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "ISteadyClock.h"

namespace skyline::service::timesrv {
    ISteadyClock::ISteadyClock(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result ISteadyClock::GetCurrentTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push(SteadyClockTimePoint{static_cast<u64>(std::time(nullptr))});
        return {};
    }
}
