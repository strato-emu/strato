// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief BaseProxy contains common functions used by most service proxies
     */
    class BaseProxy : public BaseService {
      public:
        BaseProxy(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns #ICommonStateGetter
         * @url https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter
         */
        Result GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #ISelfController
         * @url https://switchbrew.org/wiki/Applet_Manager_services#ISelfController
         */
        Result GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IWindowController
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IWindowController
         */
        Result GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IAudioController
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IAudioController
         */
        Result GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IDisplayController
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController
         */
        Result GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #ILibraryAppletCreator
         * @url https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator
         */
        Result GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IDebugFunctions
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions
         */
        Result GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns #IAppletCommonFunctions
         * @url https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions
         */
        Result GetAppletCommonFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
