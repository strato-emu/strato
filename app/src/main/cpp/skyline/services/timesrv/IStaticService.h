// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>
#include <services/timesrv/common.h>

namespace skyline::service::timesrv {
    /**
     * @brief Holds permissions for an instance of IStaticService
     */
    struct StaticServicePermissions {
        bool writeLocalSystemClock;
        bool writeUserSystemClock;
        bool writeNetworkSystemClock;
        bool writeTimezone;
        bool writeSteadyClock;
        bool ignoreUninitializedChecks;
    };

    namespace constant {
        constexpr StaticServicePermissions StaticServiceUserPermissions{};
        constexpr StaticServicePermissions StaticServiceAdminPermissions{
            .writeLocalSystemClock = true,
            .writeUserSystemClock = true,
            .writeTimezone = true,
        };
        constexpr StaticServicePermissions StaticServiceRepairPermissions{
            .writeSteadyClock = true,
        };
        constexpr StaticServicePermissions StaticServiceManagerPermissions{
            .writeLocalSystemClock = true,
            .writeUserSystemClock = true,
            .writeNetworkSystemClock = true,
            .writeTimezone = true,
            .writeSteadyClock = true,
            .ignoreUninitializedChecks = false,
        };
        constexpr StaticServicePermissions StaticServiceSystemPermissions{
            .writeNetworkSystemClock = true,
        };
        constexpr StaticServicePermissions StaticServiceSystemUpdatePermissions{
            .ignoreUninitializedChecks = true,
        };
    }

    class ITimeZoneService;
    namespace core {
        struct TimeServiceObject;
    }

    /**
     * @brief IStaticService (covers time:su, time:s) is responsible for providing the system access to various clocks
     * @url https://switchbrew.org/wiki/PSC_services#time:su.2C_time:s
     */
    class IStaticService : public BaseService {
      private:
        core::TimeServiceObject &core;
        StaticServicePermissions permissions; //!< What this instance is allowed to do

        ResultValue<ClockSnapshot> GetClockSnapshotFromSystemClockContextImpl(const SystemClockContext &userContext, const SystemClockContext &networkContext, u8 unk);

      public:
        IStaticService(const DeviceState &state, ServiceManager &manager, core::TimeServiceObject &core, StaticServicePermissions permissions);

        Result GetStandardUserSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardNetworkSystemClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetStandardSteadyClock(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result GetTimeZoneServiceIpc(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        std::shared_ptr<ITimeZoneService> GetTimeZoneService(const DeviceState &state, ServiceManager &manager);

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

        /**
         * @brief Generates an appropriate base timepoint from the supplied context
         */
        Result CalculateMonotonicSystemClockBaseTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Generates a snapshot of all clocks in the system using the current contexts
         */
        Result GetClockSnapshot(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Generates a shapshot of all clocks using the supplied contexts
         */
        Result GetClockSnapshotFromSystemClockContext(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Takes two snapshots and compares the user time between the them
         */
        Result CalculateStandardUserSystemClockDifferenceByUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Calculates the timespan between the two given clock snapshots
         */
        Result CalculateSpanBetween(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IStaticService, GetStandardUserSystemClock),
            SFUNC(0x1, IStaticService, GetStandardNetworkSystemClock),
            SFUNC(0x2, IStaticService, GetStandardSteadyClock),
            SFUNC(0x3, IStaticService, GetTimeZoneServiceIpc),
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