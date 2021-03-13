// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include "results.h"
#include "core.h"
#include "ISystemClock.h"

namespace skyline::service::timesrv {
    ISystemClock::ISystemClock(const DeviceState &state, ServiceManager &manager, core::SystemClockCore &core, bool writeClock, bool ignoreUninitializedChecks) : BaseService(state, manager), core(core), writable(writeClock), ignoreUninitializedChecks(ignoreUninitializedChecks) {}

    Result ISystemClock::GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!ignoreUninitializedChecks && !core.IsClockInitialized())
            return result::ClockUninitialized;

        auto posixTime{core.GetCurrentTime()};
        if (posixTime)
            response.Push(*posixTime);

        return posixTime;
    }

    Result ISystemClock::SetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!writable)
            return result::PermissionDenied;

        if (!ignoreUninitializedChecks && !core.IsClockInitialized())
            return result::ClockUninitialized;

        return core.SetCurrentTime(request.Pop<PosixTime>());
    }

    Result ISystemClock::GetSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!ignoreUninitializedChecks && !core.IsClockInitialized())
            return result::ClockUninitialized;

        auto context{core.GetClockContext()};
        if (context)
            response.Push(*context);

        return context;
    }

    Result ISystemClock::SetSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!writable)
            return result::PermissionDenied;

        if (!ignoreUninitializedChecks && !core.IsClockInitialized())
            return result::ClockUninitialized;

        return core.SetClockContext(request.Pop<SystemClockContext>());
    }

    Result ISystemClock::GetOperationEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!operationEvent) {
            operationEvent = std::make_shared<kernel::type::KEvent>(state, false);
            core.AddOperationEvent(operationEvent);
        }

        auto handle{state.process->InsertItem(operationEvent)};
        state.logger->Debug("ISystemClock Operation Event Handle: 0x{:X}", handle);
        response.copyHandles.push_back(handle);
        return {};
    }
}
