// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/types/KSharedMemory.h>
#include <services/serviceman.h>

namespace skyline::service::pl {
    struct SharedFontCore;

    /**
     * @brief IPlatformServiceManager is used to access shared fonts
     * @url https://switchbrew.org/wiki/Shared_Database_services#pl:u.2C_pl:s
     */
    class IPlatformServiceManager : public BaseService {
      private:
        SharedFontCore &core;

      public:
        IPlatformServiceManager(const DeviceState &state, ServiceManager &manager, SharedFontCore &core);

        /**
         * @brief Requests a shared font to be loaded
         * @url https://switchbrew.org/wiki/Shared_Database_services#RequestLoad
         */
        Result RequestLoad(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the loading state of the requested font
         * @url https://switchbrew.org/wiki/Shared_Database_services#GetLoadState
         */
        Result GetLoadState(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the size of the requested font
         * @url https://switchbrew.org/wiki/Shared_Database_services#GetSize
         */
        Result GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns the offset in shared memory of the requested font
         * @url https://switchbrew.org/wiki/Shared_Database_services#GetSharedMemoryAddressOffset
         */
        Result GetSharedMemoryAddressOffset(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to the whole font shared memory
         * @url https://switchbrew.org/wiki/Shared_Database_services#GetSharedMemoryNativeHandle
         */
        Result GetSharedMemoryNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
        * @brief Returns shared fonts information in order of priority, a bool to specify if the fonts are loaded or not and the font count
        * @url https://switchbrew.org/wiki/Shared_Database_services#GetSharedFontInOrderOfPriority
        */
        Result GetSharedFontInOrderOfPriority(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x0, IPlatformServiceManager, RequestLoad),
          SFUNC(0x1, IPlatformServiceManager, GetLoadState),
          SFUNC(0x2, IPlatformServiceManager, GetSize),
          SFUNC(0x3, IPlatformServiceManager, GetSharedMemoryAddressOffset),
          SFUNC(0x4, IPlatformServiceManager, GetSharedMemoryNativeHandle),
          SFUNC(0x5, IPlatformServiceManager, GetSharedFontInOrderOfPriority)
      )
    };
}
