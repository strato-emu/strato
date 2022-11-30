// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::mii {
    /**
     * @url https://switchbrew.org/wiki/Shared_Database_services#IDatabaseService
     */
    class IDatabaseService : public BaseService {
      public:
        IDatabaseService(const DeviceState &state, ServiceManager &manager);

        Result IsUpdated(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result Get(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result Get1(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result BuildRandom(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result BuildDefault(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result SetInterfaceVersion(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        Result DeleteFile(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IDatabaseService, IsUpdated),
            SFUNC(0x3, IDatabaseService, Get),
            SFUNC(0x4, IDatabaseService, Get1),
            SFUNC(0x6, IDatabaseService, BuildRandom),
            SFUNC(0x7, IDatabaseService, BuildDefault),
            SFUNC(0x16, IDatabaseService, SetInterfaceVersion),
            SFUNC(0x17, IDatabaseService, DeleteFile)
        )
    };
}
