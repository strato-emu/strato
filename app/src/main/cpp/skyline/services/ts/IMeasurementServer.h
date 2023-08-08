// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ts {

    class IMeasurementServer : public BaseService {
      public:
        IMeasurementServer(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#GetTemperature
         */
        Result GetTemperature(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#GetTemperatureMilliC
         */
        Result GetTemperatureMilliC(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/PTM_services#OpenSession_2
         */
        Result OpenSession(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IMeasurementServer, GetTemperature),
            SFUNC(0x3, IMeasurementServer, GetTemperatureMilliC),
            SFUNC(0x4, IMeasurementServer, OpenSession)
        )
    };
}
