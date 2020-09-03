// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::timesrv {
    /**
     * @brief This holds a steady clock timepoint (https://switchbrew.org/wiki/PSC_services#SteadyClockTimePoint)
     */
    struct SteadyClockTimePoint {
        u64 timepoint; //!< The point in time of this timepoint
        u8 id[0x10]; //!< The ID of the source clock
    };

    /**
     * @brief ISteadyClock is used to retrieve a steady time that increments uniformly for the lifetime on an application (https://switchbrew.org/wiki/PSC_services#ISteadyClock)
     */
    class ISteadyClock : public BaseService {
      public:
        ISteadyClock(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns the current value of the steady clock
         */
        Result GetCurrentTimePoint(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
