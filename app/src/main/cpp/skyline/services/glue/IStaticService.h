// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <services/timesrv/IStaticService.h>

namespace skyline::service::glue {
    /**
     * @brief IStaticService (covers time:a, time:r, time:u) is glue's extension of pcv::IStaticService, it adds some more functions and provides the user variant that most applications use
     * @url https://switchbrew.org/wiki/Glue_services#time:a.2C_time:r.2C_time:u
     */
    class IStaticService : public BaseService {
      private:
        std::shared_ptr<timesrv::IStaticService> core;
        timesrv::core::TimeServiceObject &timesrvCore;
        timesrv::StaticServicePermissions permissions;

      public:
        IStaticService(const DeviceState &state, ServiceManager &manager, std::shared_ptr<timesrv::IStaticService> core, timesrv::core::TimeServiceObject &timesrvCore, timesrv::StaticServicePermissions permissions);

        Result GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTimeZoneService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardLocalSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetEphemeralNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetStandardSteadyClockInternalOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardSteadyClockRtcValue(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetStandardUserSystemClockAutomaticCorrectionEnabled(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardUserSystemClockInitialYear(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result IsStandardNetworkSystemClockAccuracySufficient(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CalculateMonotonicSystemClockBaseTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetClockSnapshot(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetClockSnapshotFromSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CalculateStandardUserSystemClockDifferenceByUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result CalculateSpanBetween(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
                   SFUNC(0x0, IStaticService, GetStandardUserSystemClock),
                   SFUNC(0x1, IStaticService, GetStandardNetworkSystemClock),
                   SFUNC(0x2, IStaticService, GetStandardSteadyClock),
                   SFUNC(0x3, IStaticService, GetTimeZoneService),
                   SFUNC(0x4, IStaticService, GetStandardLocalSystemClock),
                   SFUNC(0x5, IStaticService, GetEphemeralNetworkSystemClock),
                   SFUNC(0x14, IStaticService, GetSharedMemoryNativeHandle),
                   SFUNC(0x32, IStaticService, SetStandardSteadyClockInternalOffset),
                   SFUNC(0x33, IStaticService, GetStandardSteadyClockRtcValue),
                   SFUNC(0x64, IStaticService, IsStandardUserSystemClockAutomaticCorrectionEnabled),
                   SFUNC(0x65, IStaticService, SetStandardUserSystemClockAutomaticCorrectionEnabled),
                   SFUNC(0x66, IStaticService, GetStandardUserSystemClockInitialYear),
                   SFUNC(0xC8, IStaticService, IsStandardNetworkSystemClockAccuracySufficient),
                   SFUNC(0xC9, IStaticService, GetStandardUserSystemClockAutomaticCorrectionUpdatedTime),
                   SFUNC(0x12C, IStaticService, CalculateMonotonicSystemClockBaseTimePoint),
                   SFUNC(0x190, IStaticService, GetClockSnapshot),
                   SFUNC(0x191, IStaticService, GetClockSnapshotFromSystemClockContext),
                   SFUNC(0x1F4, IStaticService, CalculateStandardUserSystemClockDifferenceByUser),
                   SFUNC(0x1F5, IStaticService, CalculateSpanBetween),
        )
    };
}