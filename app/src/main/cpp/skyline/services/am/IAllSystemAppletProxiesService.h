// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief IAllSystemAppletProxiesService is used to open proxies
     * @url https://switchbrew.org/wiki/Applet_Manager_services#appletAE
     */
    class IAllSystemAppletProxiesService : public BaseService {
      public:
        IAllSystemAppletProxiesService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns #ILibraryAppletProxy
         * @url https://switchbrew.org/wiki/Applet_Manager_services#OpenLibraryAppletProxy
         */
        Result OpenLibraryAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IApplicationProxy
         * @url https://switchbrew.org/wiki/Applet_Manager_services#OpenApplicationProxy
         */
        Result OpenApplicationProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IOverlayAppletProxy
         * @url https://switchbrew.org/wiki/Applet_Manager_services#OpenOverlayAppletProxy
         */
        Result OpenOverlayAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #ISystemAppletProxy
         * @url https://switchbrew.org/wiki/Applet_Manager_services#OpenSystemAppletProxy
         */
        Result OpenSystemAppletProxy(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x64, IAllSystemAppletProxiesService, OpenSystemAppletProxy),
            SFUNC(0xC8, IAllSystemAppletProxiesService, OpenLibraryAppletProxy),
            SFUNC(0xC9, IAllSystemAppletProxiesService, OpenLibraryAppletProxy),
            SFUNC(0x12C, IAllSystemAppletProxiesService, OpenOverlayAppletProxy),
            SFUNC(0x15E, IAllSystemAppletProxiesService, OpenApplicationProxy)
        )
    };
}
