// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include "IDisplayService.h"
#include "IRootService.h"

namespace skyline::service::visrv {
    /**
     * @brief This is used by applications to access the display
     * @url https://switchbrew.org/wiki/Display_services#IApplicationDisplayService
     */
    class IApplicationDisplayService : public IDisplayService {
      private:
        PrivilegeLevel level;

        /**
         * @brief Specifies the method to scale up the layer content to its bounds
         */
        enum class ScalingMode : u64 {
            Freeze = 0,
            ScaleToLayer = 1,
            ScaleAndCrop = 2,
            None = 3,
            PreserveAspectRatio = 4,
        };

      public:
        IApplicationDisplayService(const DeviceState &state, ServiceManager &manager, PrivilegeLevel level);

        /**
         * @brief Returns an handle to the 'nvnflinger' service
         * @url https://switchbrew.org/wiki/Display_services#GetRelayService
         */
        Result GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to the 'nvnflinger' service
         * @url https://switchbrew.org/wiki/Display_services#GetIndirectDisplayTransactionService
         */
        Result GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #ISystemDisplayService
         * @url https://switchbrew.org/wiki/Display_services#GetSystemDisplayService
         */
        Result GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #IManagerDisplayService
         * @url https://switchbrew.org/wiki/Display_services#GetManagerDisplayService
         */
        Result GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @url https://switchbrew.org/wiki/Display_services#ListDisplays
         */
        Result ListDisplays(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens up a display using its name as the input
         * @url https://switchbrew.org/wiki/Display_services#OpenDisplay
         */
        Result OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes an open display using its ID
         * @url https://switchbrew.org/wiki/Display_services#CloseDisplay
         */
        Result CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens a specific layer on a display
         * @url https://switchbrew.org/wiki/Display_services#OpenLayer
         */
        Result OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes a specific layer on a display
         * @url https://switchbrew.org/wiki/Display_services#CloseLayer
         */
        Result CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the scaling mode for a window, this is not required by emulators
         * @url https://switchbrew.org/wiki/Display_services#SetLayerScalingMode
         */
        Result SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Draws an indirect layer into the supplied buffer
         * @url https://switchbrew.org/wiki/Display_services#GetIndirectLayerImageMap
         */
        Result GetIndirectLayerImageMap(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Gets the amount of memory required for an indirect layer
         * @url https://switchbrew.org/wiki/Display_services#GetIndirectLayerImageRequiredMemoryInfo
         */
        Result GetIndirectLayerImageRequiredMemoryInfo(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Converts an arbitrary scaling mode to a VI scaling mode
         */
        Result ConvertScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a KEvent which is triggered every time a frame is drawn
         * @url https://switchbrew.org/wiki/Display_services#GetDisplayVsyncEvent
         */
        Result GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

      SERVICE_DECL(
          SFUNC(0x64, IApplicationDisplayService, GetRelayService),
          SFUNC(0x65, IApplicationDisplayService, GetSystemDisplayService),
          SFUNC(0x66, IApplicationDisplayService, GetManagerDisplayService),
          SFUNC(0x67, IApplicationDisplayService, GetIndirectDisplayTransactionService),
          SFUNC(0x3E8, IApplicationDisplayService, ListDisplays),
          SFUNC(0x3F2, IApplicationDisplayService, OpenDisplay),
          SFUNC(0x3FC, IApplicationDisplayService, CloseDisplay),
          SFUNC(0x7E4, IApplicationDisplayService, OpenLayer),
          SFUNC(0x7E5, IApplicationDisplayService, CloseLayer),
          SFUNC_BASE(0x7EE, IApplicationDisplayService, IDisplayService, CreateStrayLayer),
          SFUNC_BASE(0x7EF, IApplicationDisplayService, IDisplayService, DestroyStrayLayer),
          SFUNC(0x835, IApplicationDisplayService, SetLayerScalingMode),
          SFUNC(0x836, IApplicationDisplayService, ConvertScalingMode),
          SFUNC(0x992, IApplicationDisplayService, GetIndirectLayerImageMap),
          SFUNC(0x99C, IApplicationDisplayService, GetIndirectLayerImageRequiredMemoryInfo),
          SFUNC(0x1452, IApplicationDisplayService, GetDisplayVsyncEvent)
      )
    };
}
