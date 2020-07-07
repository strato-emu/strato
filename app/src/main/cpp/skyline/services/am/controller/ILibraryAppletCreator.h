// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator
     */
    class ILibraryAppletCreator : public BaseService {
      public:
        ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This function returns a handle to a library applet accessor (https://switchbrew.org/wiki/Applet_Manager_services#CreateLibraryApplet)
         */
        void CreateLibraryApplet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function creates an IStorage that can be used by the application (https://switchbrew.org/wiki/Applet_Manager_services#CreateStorage)
         */
        void CreateStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
