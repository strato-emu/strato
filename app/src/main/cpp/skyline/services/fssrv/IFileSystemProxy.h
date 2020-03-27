// SPDX-License-Identifier: LGPL-3.0-or-later
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include "IFileSystem.h"

namespace skyline::service::fssrv {
    /**
     * @brief IFileSystemProxy or fsp-srv is responsible for providing handles to file systems (https://switchbrew.org/wiki/Filesystem_services#fsp-srv)
     */
    class IFileSystemProxy : public BaseService {
      public:
        pid_t process{}; //!< This holds the PID set by SetCurrentProcess

        IFileSystemProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This sets the PID of the process using FS currently (https://switchbrew.org/wiki/Filesystem_services#SetCurrentProcess)
         */
        void SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns a handle to an instance of #IFileSystem (https://switchbrew.org/wiki/Filesystem_services#IFileSystem) with type SDCard
         */
        void OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
