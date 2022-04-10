// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>
#include "IApplet.h"
#include <applet/applet_creator.h>

namespace skyline::service::am {
    namespace result {
        constexpr Result ObjectInvalid(128, 500);
        constexpr Result OutOfBounds(128, 503);
        constexpr Result NotAvailable(128, 2);
    }

    /**
     * @brief ILibraryAppletAccessor is used to communicate with the library applet
     * @url https://switchbrew.org/wiki/Applet_Manager_services#ILibraryAppletAccessor
     */
    class ILibraryAppletAccessor : public BaseService {
      private:
        std::shared_ptr<kernel::type::KEvent> stateChangeEvent;
        std::shared_ptr<kernel::type::KEvent> popNormalOutDataEvent;
        std::shared_ptr<kernel::type::KEvent> popInteractiveOutDataEvent;

        KHandle stateChangeEventHandle{};
        KHandle popNormalOutDataEventHandle{};
        KHandle popInteractiveOutDataEventHandle{};

        std::shared_ptr<IApplet> applet;

      public:
        ILibraryAppletAccessor(const DeviceState &state, ServiceManager &manager, skyline::applet::AppletId appletId, applet::LibraryAppletMode appletMode);

        /**
         * @brief Returns a handle to the library applet state change event
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetAppletStateChangedEvent
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

        /**
         * @brief Pushes in data to the library applet, through the interactive queue
         * @url https://switchbrew.org/wiki/Applet_Manager_services#PushInteractiveInData
         */
        Result PushInteractiveInData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Receives data from the library applet, from the interactive queue
         * @url https://switchbrew.org/wiki/Applet_Manager_services#PopInteractiveOutData
         */
        Result PopInteractiveOutData(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets te KEvent for when there's data to be popped by the guest on the normal queue
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetPopOutDataEvent
         */
        Result GetPopOutDataEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets the KEvent for when there's data to be popped by the guest on the interactive queue
         * @url https://switchbrew.org/wiki/Applet_Manager_services#GetPopInteractiveOutDataEvent
         */
        Result GetPopInteractiveOutDataEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, ILibraryAppletAccessor, GetAppletStateChangedEvent),
            SFUNC(0xA, ILibraryAppletAccessor, Start),
            SFUNC(0x1E, ILibraryAppletAccessor, GetResult),
            SFUNC(0x64, ILibraryAppletAccessor, PushInData),
            SFUNC(0x65, ILibraryAppletAccessor, PopOutData),
            SFUNC(0x67, ILibraryAppletAccessor, PushInteractiveInData),
            SFUNC(0x68, ILibraryAppletAccessor, PopInteractiveOutData),
            SFUNC(0x69, ILibraryAppletAccessor, GetPopOutDataEvent),
            SFUNC(0x6A, ILibraryAppletAccessor, GetPopInteractiveOutDataEvent)
        )
    };
}
