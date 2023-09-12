// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::capsrv {
    /**
     * @url https://switchbrew.org/wiki/Capture_services#caps:c
     */
    class ICaptureControllerService : public BaseService {
      public:
        ICaptureControllerService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Capture_services#Cmd33
         */
        Result SetShimLibraryVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x21, ICaptureControllerService, SetShimLibraryVersion)
        )
    };
}
