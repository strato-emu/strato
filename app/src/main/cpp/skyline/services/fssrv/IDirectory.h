// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <vfs/directory.h>
#include <vfs/filesystem.h>

namespace skyline::service::fssrv {
    /**
     * @brief IDirectory is an interface for accessing directory contents
     * @url https://switchbrew.org/wiki/Filesystem_services#IDirectory
     */
    class IDirectory : public BaseService {
      private:
        std::shared_ptr<vfs::Directory> backing; //!< Backing directory of the IDirectory
        std::shared_ptr<vfs::FileSystem> backingFs; //!< Backing filesystem of the IDirectory
        u32 remainingReadCount{};

      public:
        IDirectory(std::shared_ptr<vfs::Directory> backing, std::shared_ptr<vfs::FileSystem> backingFs, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Reads the contents of an IDirectory
         */
        Result Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Filesystem_services#GetEntryCount
         */
        Result GetEntryCount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IDirectory, Read),
            SFUNC(0x1, IDirectory, GetEntryCount)
        )
    };
}
