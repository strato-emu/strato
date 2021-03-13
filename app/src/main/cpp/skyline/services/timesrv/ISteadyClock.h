// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::timesrv {
    namespace core {
        class SteadyClockCore;
    }

    /**
     * @brief ISteadyClock is used to interface with timesrv steady clocks
     * @url https://switchbrew.org/wiki/PSC_services#ISteadyClock
     */
    class ISteadyClock : public BaseService {
      private:
        core::SteadyClockCore &core;
        bool writeable; //!< If this instance can set the test offset
        bool ignoreUninitializedChecks;

      public:
        ISteadyClock(const DeviceState &state, ServiceManager &manager, core::SteadyClockCore &core, bool writeable, bool ignoreUninitializedChecks);

        Result GetCurrentTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTestOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetTestOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetRtcValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsRtcResetDetected(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetSetupResultValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetInternalOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ISteadyClock, GetCurrentTimePoint),
            SFUNC(0x2, ISteadyClock, GetTestOffset),
            SFUNC(0x3, ISteadyClock, SetTestOffset),
            SFUNC(0x64, ISteadyClock, GetRtcValue),
            SFUNC(0x65, ISteadyClock, IsRtcResetDetected),
            SFUNC(0x66, ISteadyClock, GetSetupResultValue),
            SFUNC(0xC8, ISteadyClock, GetInternalOffset),
        )
    };
}