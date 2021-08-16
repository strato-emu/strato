// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>

namespace skyline::service::spl {
    /**
     * @brief csrng provides cryptographically secure random number generation
     * @url https://switchbrew.org/wiki/SPL_services#csrng
     */
    class IRandomInterface : public BaseService {
      public:
        IRandomInterface(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Fills an output buffer with random bytes
         */
        Result GetRandomBytes(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x0, IRandomInterface, GetRandomBytes)
        )
    };
}
