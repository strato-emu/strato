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
         * @brief This returns #ICommonStateGetter (https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter)
         */
        Result GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ISelfController (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
         */
        Result GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IWindowController (https://switchbrew.org/wiki/Applet_Manager_services#IWindowController)
         */
        Result GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAudioController (https://switchbrew.org/wiki/Applet_Manager_services#IAudioController)
         */
        Result GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDisplayController (https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController)
         */
        Result GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ILibraryAppletCreator (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator)
         */
        Result GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDebugFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
         */
        Result GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAppletCommonFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions)
         */
        Result GetAppletCommonFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
