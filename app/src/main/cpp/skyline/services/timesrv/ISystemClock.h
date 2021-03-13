// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    namespace core {
        class SystemClockCore;
    }

    /**
     * @brief ISystemClock is used to interface with timesrv system clocks
     * @url https://switchbrew.org/wiki/PSC_services#ISystemClock
     */
    class ISystemClock : public BaseService {
      private:
        core::SystemClockCore &core;
        bool writable; //!< If this instance can set the clock time
        bool ignoreUninitializedChecks;

        std::shared_ptr<kernel::type::KEvent> operationEvent{};

      public:
        ISystemClock(const DeviceState &state, ServiceManager &manager, core::SystemClockCore &core, bool writeClock, bool ignoreUninitializedChecks);

        Result GetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetCurrentTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetOperationEventReadableHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ISystemClock, GetCurrentTime),
            SFUNC(0x1, ISystemClock, SetCurrentTime),
            SFUNC(0x2, ISystemClock, GetSystemClockContext),
            SFUNC(0x3, ISystemClock, SetSystemClockContext),
            SFUNC(0x4, ISystemClock, GetOperationEventReadableHandle),
        )
    };
}
