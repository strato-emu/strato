// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <common/uuid.h>
#include <kernel/types/KEvent.h>
#include "common.h"
#include "IStaticService.h"

namespace skyline::service::timesrv {
    namespace core {
        struct TimeServiceObject;
    }

    class TimeManagerServer {
      private:
        core::TimeServiceObject &core;

      public:
        TimeManagerServer(core::TimeServiceObject &core);

        std::shared_ptr<IStaticService> GetUserStaticService(const DeviceState &state, ServiceManager &manager);

        std::shared_ptr<IStaticService> GetAdminStaticService(const DeviceState &state, ServiceManager &manager);

        std::shared_ptr<IStaticService> GetRepairStaticService(const DeviceState &state, ServiceManager &manager);

        std::shared_ptr<IStaticService> GetManagerStaticService(const DeviceState &state, ServiceManager &manager);

        Result SetupStandardSteadyClock(UUID rtcId, TimeSpanType rtcOffset, TimeSpanType internalOffset, TimeSpanType testOffset, bool rtcResetDetected);


        Result SetupTimeZoneManager(std::string_view locationName, const SteadyClockTimePoint &updateTime, size_t locationCount, std::array<u8, 0x10> binaryVersion, span<u8> binary);

        Result SetupStandardLocalSystemClock(const SystemClockContext &context, PosixTime posixTime);

        Result SetupStandardNetworkSystemClock(const SystemClockContext &context, TimeSpanType sufficientAccuracy);

        Result SetupStandardUserSystemClock(bool enableAutomaticCorrection, const SteadyClockTimePoint &automaticCorrectionUpdateTime);

        Result SetupEphemeralSystemClock();

        std::shared_ptr<kernel::type::KEvent> GetStandardUserSystemClockAutomaticCorrectionEvent();

        Result SetStandardSteadyClockRtcOffset(TimeSpanType rtcOffset);
    };
}
