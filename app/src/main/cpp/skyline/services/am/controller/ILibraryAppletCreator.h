// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator
     */
    class ILibraryAppletCreator : public BaseService {
      public:
        ILibraryAppletCreator(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a handle to a library applet accessor
         * @url https://switchbrew.org/wiki/Applet_Manager_services#CreateLibraryApplet
         */
        Result CreateLibraryApplet(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates an IStorage that can be used by the application, backed by service-allocated memory
         * @url https://switchbrew.org/wiki/Applet_Manager_services#CreateStorage
         */
        Result CreateStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Creates an IStorage that can be used by the application, backed by the supplied transfer memory
         * @url https://switchbrew.org/wiki/Applet_Manager_services#CreateTransferMemoryStorage
         */
        Result CreateTransferMemoryStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILibraryAppletCreator, CreateLibraryApplet),
            SFUNC(0xA, ILibraryAppletCreator, CreateStorage),
            SFUNC(0xB, ILibraryAppletCreator, CreateTransferMemoryStorage)
        )
    };
}
