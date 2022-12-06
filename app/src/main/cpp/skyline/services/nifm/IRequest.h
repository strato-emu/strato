// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KEvent.h>
#include <services/serviceman.h>

namespace skyline::service::nifm {
    /**
     * @brief IRequest is used by applications to bring up a network
     * @url https://switchbrew.org/wiki/Network_Interface_services#IRequest
     */
    class IRequest : public BaseService {
      private:
        std::shared_ptr<type::KEvent> event0; //!< The KEvent that is signalled on request state changes
        std::shared_ptr<type::KEvent> event1; //!< The KEvent that is signalled on request changes

      public:
        IRequest(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns the current state of the request
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetRequestState
         */
        Result GetRequestState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the error code if a network bring up request fails
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetResult
         */
        Result GetResult(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns two KEvent handles that signal request on request updates
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetSystemEventReadableHandles
         */
        Result GetSystemEventReadableHandles(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Submits a request to bring up a network
         * @url https://switchbrew.org/wiki/Network_Interface_services#Submit
         */
        Result Submit(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#SetConnectionConfirmationOption
         */
        Result SetConnectionConfirmationOption(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Network_Interface_services#GetAppletInfo
         */
        Result GetAppletInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IRequest, GetRequestState),
            SFUNC(0x1, IRequest, GetResult),
            SFUNC(0x2, IRequest, GetSystemEventReadableHandles),
            SFUNC(0x4, IRequest, Submit),
            SFUNC(0xB, IRequest, SetConnectionConfirmationOption),
            SFUNC(0x15, IRequest, GetAppletInfo)
      )
    };
}
