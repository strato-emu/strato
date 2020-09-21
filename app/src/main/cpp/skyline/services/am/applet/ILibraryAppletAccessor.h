// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/base_service.h>
#include <services/serviceman.h>

namespace skyline::service::am {
    /**
     * @brief ILibraryAppletAccessor is used to communicate with the library applet (https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletAccessor)
     */
    class ILibraryAppletAccessor : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> stateChangeEvent; //!< This KEvent is triggered when the applet's state changes

      public:
        ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This function returns a handle to the library applet state change event
         */
        Result GetAppletStateChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function starts the library applet (https://switchbrew.org/wiki/Applet_Manager_services#Start)
         */
        Result Start(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function returns the exit code of the library applet (https://switchbrew.org/wiki/Applet_Manager_services#GetResult)
         */
        Result GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function pushes in data to the library applet (https://switchbrew.org/wiki/Applet_Manager_services#PushInData)
         */
        Result PushInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This function receives data from the library applet (https://switchbrew.org/wiki/Applet_Manager_services#PopOutData)
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
