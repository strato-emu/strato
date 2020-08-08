// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <vfs/backing.h>

namespace skyline::service::fssrv {
    /**
     * @brief IFile is an interface for accessing files (https://switchbrew.org/wiki/Filesystem_services#IFile)
     */
    class IFile : public BaseService {
      private:
        std::shared_ptr<vfs::Backing> backing; //!< The backing of the IFile

      public:
        IFile(std::shared_ptr<vfs::Backing> &backing, const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This reads a buffer from a region of an IFile
         */
        void Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This writes a buffer to a region of an IFile
         */
        void Write(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This sets the size of an IFile
         */
        void SetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This obtains the size of an IFile
         */
        void GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
