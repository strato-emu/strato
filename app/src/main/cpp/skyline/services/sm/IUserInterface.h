// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/serviceman.h>

namespace skyline::service::sm {
    namespace result {
        constexpr Result OutOfProcesses(21, 1);
        constexpr Result InvalidClient(21, 2);
        constexpr Result OutOfSessions(21, 3);
        constexpr Result AlreadyRegistered(21, 4);
        constexpr Result OutOfServices(21, 5);
        constexpr Result InvalidServiceName(21, 6);
        constexpr Result NotRegistered(21, 7);
        constexpr Result NotAllowed(21, 8);
        constexpr Result TooLargeAccessControl(21, 9);
    }

    /**
     * @brief IUserInterface or sm: is responsible for providing handles to services
     * @url https://switchbrew.org/wiki/Services_API
     */
    class IUserInterface : public BaseService {
      public:
        IUserInterface(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Initializes the sm: service.
         * @url https://switchbrew.org/wiki/Services_API#Initialize
         */
        Result Initialize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a service with its name passed in as an argument
         * @url https://switchbrew.org/wiki/Services_API#GetService
         */
        Result GetService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IUserInterface, Initialize),
            SFUNC_TIPC(0x10, IUserInterface, Initialize),
            SFUNC(0x1, IUserInterface, GetService),
            SFUNC_TIPC(0x11, IUserInterface, GetService)
        )
    };
}
