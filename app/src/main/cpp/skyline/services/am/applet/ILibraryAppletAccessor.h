// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief ILibraryAppletAccessor is used to communicate with the library applet
     * @url https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletAccessor
     */
    class ILibraryAppletAccessor : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> stateChangeEvent; //!< This KEvent is triggered when the applet's state changes

      public:
        ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns a handle to the library applet state change event
         */
        Result GetAppletStateChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Starts the library applet
         * @url https://switchbrew.org/wiki/Applet_Manager_services#Start
         */
        Result Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the exit code of the library applet
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetResult
         */
        Result GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Pushes in data to the library applet
         * @url https://switchbrew.org/wiki/Applet_Manager_services#PushInData
         */
        Result PushInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Receives data from the library applet
         * @url https://switchbrew.org/wiki/Applet_Manager_services#PopOutData
         */
        Result PopOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILibraryAppletAccessor, GetAppletStateChangedEvent),
            SFUNC(0xA, ILibraryAppletAccessor, Start),
            SFUNC(0x1E, ILibraryAppletAccessor, GetResult),
            SFUNC(0x64, ILibraryAppletAccessor, PushInData),
            SFUNC(0x65, ILibraryAppletAccessor, PopOutData)
        )
    };
}
