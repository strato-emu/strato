// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::prepo {
    /**
     * @brief IPrepoService or prepo:u is used by applications to store statistics
     * @url https://switchbrew.org/wiki/BCAT_services#prepo:a.2C_prepo:a2.2C_prepo:m.2C_prepo:u.2C_prepo:s
     */
    class IPrepoService : public BaseService {
      public:
        IPrepoService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Saves a play report for the given user
         */
        Result SaveReportWithUserOld (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SaveReportWithUserOld2 (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SaveReport (type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SaveReportWithUser(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result RequestImmediateTransmission(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x2775, IPrepoService, SaveReportWithUserOld),
            SFUNC(0x2777, IPrepoService, SaveReportWithUserOld2),
            SFUNC(0x2778, IPrepoService, SaveReport),
            SFUNC(0x2779, IPrepoService, SaveReportWithUser),
            SFUNC(0x27D8, IPrepoService, RequestImmediateTransmission)
        )
    };
}
