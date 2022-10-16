// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::ldn {
    namespace result {
        static constexpr Result DeviceDisabled{203, 22};
    }

    /**
     * @brief IUserLocalCommunicationService is used by applications to manage LDN sessions
     * @url https://switchbrew.org/wiki/LDN_services#IUserLocalCommunicationService
     */
    class IUserLocalCommunicationService : public BaseService {
      private:
        std::shared_ptr<type::KEvent> event; //!< The KEvent that is signalled on state changes

      public:
        IUserLocalCommunicationService(const DeviceState &state, ServiceManager &manager);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#GetState
         */
        Result GetState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#AttachStateChangeEvent
         */
        Result AttachStateChangeEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#InitializeSystem
         */
        Result InitializeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#FinalizeSystem
         */
        Result FinalizeSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/LDN_services#InitializeSystem2
         */
        Result InitializeSystem2(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
            SFUNC(0x0, IUserLocalCommunicationService, GetState),
            SFUNC(0x64, IUserLocalCommunicationService, AttachStateChangeEvent),
            SFUNC(0x190, IUserLocalCommunicationService, InitializeSystem),
            SFUNC(0x191, IUserLocalCommunicationService, FinalizeSystem),
            SFUNC(0x192, IUserLocalCommunicationService, InitializeSystem2),
        )
    };
}
