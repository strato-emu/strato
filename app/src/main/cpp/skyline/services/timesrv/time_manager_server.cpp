// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <common.h>
#include "core.h"
#include "IStaticService.h"
#include "time_manager_server.h"

namespace skyline::service::timesrv {
    TimeManagerServer::TimeManagerServer(core::TimeServiceObject &core) : core(core) {}

    std::shared_ptr<IStaticService> TimeManagerServer::GetStaticServiceAsUser(const DeviceState &state, ServiceManager &manager) {
        return std::make_shared<IStaticService>(state, manager, core, constant::StaticServiceUserPermissions);
    }

    std::shared_ptr<IStaticService> TimeManagerServer::GetStaticServiceAsAdmin(const DeviceState &state, ServiceManager &manager) {
        return std::make_shared<IStaticService>(state, manager, core, constant::StaticServiceAdminPermissions);
    }

    std::shared_ptr<IStaticService> TimeManagerServer::GetStaticServiceAsRepair(const DeviceState &state, ServiceManager &manager) {
        return std::make_shared<IStaticService>(state, manager, core, constant::StaticServiceRepairPermissions);
    }

    std::shared_ptr<IStaticService> TimeManagerServer::GetStaticServiceAsServiceManager(const DeviceState &state, ServiceManager &manager) {
        return std::make_shared<IStaticService>(state, manager, core, constant::StaticServiceManagerPermissions);
    }

    Result TimeManagerServer::SetupStandardSteadyClock(UUID rtcId, TimeSpanType rtcOffset, TimeSpanType internalOffset, TimeSpanType testOffset, bool rtcResetDetected) {
        core.standardSteadyClock.Setup(rtcId, rtcOffset, internalOffset, testOffset, rtcResetDetected);

        auto timePoint{core.standardSteadyClock.GetCurrentRawTimePoint()};
        core.timeSharedMemory.SetupStandardSteadyClock(rtcId, timePoint);
        return {};
    }

    Result TimeManagerServer::SetupTimeZoneManager(std::string_view locationName, const SteadyClockTimePoint &updateTime, size_t locationCount, std::array<u8, 0x10> binaryVersion, span<u8> binary) {
        return core.timeZoneManager.Setup(locationName, updateTime, locationCount, binaryVersion, binary);
    }

    Result TimeManagerServer::SetupStandardLocalSystemClock(const SystemClockContext &context, PosixTime posixTime) {
        core.localSystemClock.SetUpdateCallback(&core.localSystemClockContextWriter);
        core.localSystemClock.Setup(context, posixTime);
        return {};
    }

    Result TimeManagerServer::SetupStandardNetworkSystemClock(const SystemClockContext &context, TimeSpanType sufficientAccuracy) {
        core.networkSystemClock.SetUpdateCallback(&core.networkSystemClockContextWriter);
        core.networkSystemClock.Setup(context, sufficientAccuracy);
        return {};
    }

    Result TimeManagerServer::SetupStandardUserSystemClock(bool enableAutomaticCorrection, const SteadyClockTimePoint &automaticCorrectionUpdateTime) {
        core.userSystemClock.Setup(enableAutomaticCorrection, automaticCorrectionUpdateTime);
        return {};
    }

    Result TimeManagerServer::SetupEphemeralSystemClock() {
        core.empheralSystemClock.SetUpdateCallback(&core.ephemeralNetworkSystemClockContextWriter);
        core.empheralSystemClock.Setup();
        return {};
    }

    std::shared_ptr<kernel::type::KEvent> TimeManagerServer::GetStandardUserSystemClockAutomaticCorrectionEvent() {
        return core.userSystemClock.automaticCorrectionUpdatedEvent;
    }

    Result TimeManagerServer::SetStandardSteadyClockRtcOffset(TimeSpanType rtcOffset) {
        core.standardSteadyClock.SetRtcOffset(rtcOffset);
        core.timeSharedMemory.SetSteadyClockRawTimePoint(core.standardSteadyClock.GetCurrentRawTimePoint());

        return {};
    }
}
