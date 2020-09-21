// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IDisplayService.h"

namespace skyline::service::visrv {
    /**
     * @brief This service is used to access the display (https://switchbrew.org/wiki/Display_services#IApplicationDisplayService)
     */
    class IApplicationDisplayService : public IDisplayService {
      public:
        IApplicationDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Returns an handle to the 'nvnflinger' service (https://switchbrew.org/wiki/Display_services#GetRelayService)
         */
        Result GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to the 'nvnflinger' service (https://switchbrew.org/wiki/Display_services#GetIndirectDisplayTransactionService)
         */
        Result GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #ISystemDisplayService (https://switchbrew.org/wiki/Display_services#GetSystemDisplayService)
         */
        Result GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #IManagerDisplayService (https://switchbrew.org/wiki/Display_services#GetManagerDisplayService)
         */
        Result GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens up a display using it's name as the input (https://switchbrew.org/wiki/Display_services#OpenDisplay)
         */
        Result OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes an open display using it's ID (https://switchbrew.org/wiki/Display_services#CloseDisplay)
         */
        Result CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens a specific layer on a display (https://switchbrew.org/wiki/Display_services#OpenLayer)
         */
        Result OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes a specific layer on a display (https://switchbrew.org/wiki/Display_services#CloseLayer)
         */
        Result CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the scaling mode for a window, this is not required by emulators (https://switchbrew.org/wiki/Display_services#SetLayerScalingMode)
         */
        Result SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a KEvent which is triggered every time a frame is drawn (https://switchbrew.org/wiki/Display_services#GetDisplayVsyncEvent)
         */
        Result GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        SERVICE_DECL(
            SFUNC(0x64, IApplicationDisplayService, GetRelayService),
            SFUNC(0x65, IApplicationDisplayService, GetSystemDisplayService),
            SFUNC(0x66, IApplicationDisplayService, GetManagerDisplayService),
            SFUNC(0x67, IApplicationDisplayService, GetIndirectDisplayTransactionService),
            SFUNC(0x3F2, IApplicationDisplayService, OpenDisplay),
            SFUNC(0x3FC, IApplicationDisplayService, CloseDisplay),
            SFUNC(0x7E4, IApplicationDisplayService, OpenLayer),
            SFUNC(0x7E5, IApplicationDisplayService, CloseLayer),
            SFUNC_BASE(0x7EE, IApplicationDisplayService, IDisplayService, CreateStrayLayer),
            SFUNC_BASE(0x7EF, IApplicationDisplayService, IDisplayService, DestroyStrayLayer),
            SFUNC(0x835, IApplicationDisplayService, SetLayerScalingMode),
            SFUNC(0x1452, IApplicationDisplayService, GetDisplayVsyncEvent)
        )
    };
}
