// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief BaseProxy contains common functions used by most service proxies
     */
    class BaseProxy : public BaseService {
      public:
        BaseProxy(const DeviceState &state, ServiceManager &manager, const std::unordered_map<u32, std::function<void(type::KSession & , ipc::IpcRequest & , ipc::IpcResponse & )>> &vTable);

        /**
         * @brief This returns #ICommonStateGetter (https://switchbrew.org/wiki/Applet_Manager_services#ICommonStateGetter)
         */
        void GetCommonStateGetter(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ISelfController (https://switchbrew.org/wiki/Applet_Manager_services#ISelfController)
         */
        void GetSelfController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IWindowController (https://switchbrew.org/wiki/Applet_Manager_services#IWindowController)
         */
        void GetWindowController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAudioController (https://switchbrew.org/wiki/Applet_Manager_services#IAudioController)
         */
        void GetAudioController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDisplayController (https://switchbrew.org/wiki/Applet_Manager_services#IDisplayController)
         */
        void GetDisplayController(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #ILibraryAppletCreator (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletCreator)
         */
        void GetLibraryAppletCreator(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IDebugFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IDebugFunctions)
         */
        void GetDebugFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This returns #IAppletCommonFunctions (https://switchbrew.org/wiki/Applet_Manager_services#IAppletCommonFunctions)
         */
        void GetAppletCommonFunctions(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
