// SPDX-License-Identifier: MPL-2.0
// Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::fssrv {
    /**
     * @url https://switchbrew.org/wiki/Filesystem_services#IMultiCommitManager
     */
    class IMultiCommitManager : public BaseService {
      public:
        IMultiCommitManager(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/Filesystem_services#Add
         */
        Result Add(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Filesystem_services#Commit
         */
        Result Commit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x1, IMultiCommitManager, Add),
            SFUNC(0x2, IMultiCommitManager, Commit)
        )
    };
}
