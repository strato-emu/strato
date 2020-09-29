// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "KSyncObject.h"

namespace skyline::service {
    class BaseService;
}

namespace skyline::kernel::type {
    /**
     * @brief KService holds a reference to a service, this is equivalent to KClientSession
     */
    class KSession : public KSyncObject {
      public:
        std::shared_ptr<service::BaseService> serviceObject;
        std::vector<std::shared_ptr<service::BaseService>> domains; //!< A vector of services that correspond to virtual handles
        KHandle handleIndex{}; //!< The currently allocated handle index
        bool isOpen{true}; //!< If the session is open or not
        bool isDomain{}; //!< If this is a domain session or not

        /**
         * @param serviceObject A shared pointer to the service class
         */
        KSession(const DeviceState &state, std::shared_ptr<service::BaseService> &serviceObject) : serviceObject(serviceObject), KSyncObject(state, KType::KSession) {}

        /**
         * @brief Converts this session into a domain session
         * @url https://switchbrew.org/wiki/IPC_Marshalling#Domains
         * @return The virtual handle of this service in the domain
         */
        KHandle ConvertDomain() {
            isDomain = true;
            domains.push_back(serviceObject);
            return handleIndex++;
        }
    };
}
