// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <common/uuid.h>
#include <kernel/types/KEvent.h>
#include <services/serviceman.h>
#include "common.h"

namespace skyline::service::timesrv {
    namespace core {
        struct TimeServiceObject;
    }

    class IStaticService;

    /**
     * @brief time:m (we don't expose this as IPC as games don't use it) is used to manage the state of timesrv
     * @url https://switchbrew.org/w/index.php?title=PSC_services#time:m
     */
    class TimeManagerServer {
      private:
        core::TimeServiceObject &core;

      public:
        TimeManagerServer(core::TimeServiceObject &core);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#GetStaticServiceAsUser
         */
        std::shared_ptr<IStaticService> GetStaticServiceAsUser(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#GetStaticServiceAsAdmin
         */
        std::shared_ptr<IStaticService> GetStaticServiceAsAdmin(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#GetStaticServiceAsRepair
         */
        std::shared_ptr<IStaticService> GetStaticServiceAsRepair(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#GetStaticServiceAsServiceManager
         */
        std::shared_ptr<IStaticService> GetStaticServiceAsServiceManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupStandardSteadyClockCore
         */
        Result SetupStandardSteadyClock(UUID rtcId, TimeSpanType rtcOffset, TimeSpanType internalOffset, TimeSpanType testOffset, bool rtcResetDetected);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupStandardLocalSystemClockCore
         */
        Result SetupStandardLocalSystemClock(const SystemClockContext &context, PosixTime posixTime);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupStandardNetworkSystemClockCore
         */
        Result SetupStandardNetworkSystemClock(const SystemClockContext &context, TimeSpanType sufficientAccuracy);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupStandardUserSystemClockCore
         */
        Result SetupStandardUserSystemClock(bool enableAutomaticCorrection, const SteadyClockTimePoint &automaticCorrectionUpdateTime);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupTimeZoneServiceCore
         */
        Result SetupTimeZoneManager(std::string_view locationName, const SteadyClockTimePoint &updateTime, size_t locationCount, std::array<u8, 0x10> binaryVersion, span<u8> binary);

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetupEphemeralNetworkSystemClockCore
         */
        Result SetupEphemeralSystemClock();

        std::shared_ptr<kernel::type::KEvent> GetStandardUserSystemClockAutomaticCorrectionEvent();

        /**
         * @url https://switchbrew.org/w/index.php?title=PSC_services#SetStandardSteadyClockBaseTime
         */
        Result SetStandardSteadyClockRtcOffset(TimeSpanType rtcOffset);
    };
}
