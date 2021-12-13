// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::glue {
    /**
     * @brief Stub implementation for IWriterForSystem
     * @url https://switchbrew.org/wiki/Glue_services#ectx:w
     */
    class IWriterForSystem : public BaseService {
      public:
        IWriterForSystem(const DeviceState &state, ServiceManager &manager);

        Result CreateContextRegistrar(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IWriterForSystem, CreateContextRegistrar),
        )
    };
}
