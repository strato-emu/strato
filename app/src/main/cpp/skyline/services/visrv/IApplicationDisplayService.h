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
        void GetRelayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to the 'nvnflinger' service (https://switchbrew.org/wiki/Display_services#GetIndirectDisplayTransactionService)
         */
        void GetIndirectDisplayTransactionService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #ISystemDisplayService (https://switchbrew.org/wiki/Display_services#GetSystemDisplayService)
         */
        void GetSystemDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns an handle to #IManagerDisplayService (https://switchbrew.org/wiki/Display_services#GetManagerDisplayService)
         */
        void GetManagerDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens up a display using it's name as the input (https://switchbrew.org/wiki/Display_services#OpenDisplay)
         */
        void OpenDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes an open display using it's ID (https://switchbrew.org/wiki/Display_services#CloseDisplay)
         */
        void CloseDisplay(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Opens a specific layer on a display (https://switchbrew.org/wiki/Display_services#OpenLayer)
         */
        void OpenLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Closes a specific layer on a display (https://switchbrew.org/wiki/Display_services#CloseLayer)
         */
        void CloseLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Sets the scaling mode for a window, this is not required by emulators (https://switchbrew.org/wiki/Display_services#SetLayerScalingMode)
         */
        void SetLayerScalingMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Returns a handle to a KEvent which is triggered every time a frame is drawn (https://switchbrew.org/wiki/Display_services#GetDisplayVsyncEvent)
         */
        void GetDisplayVsyncEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
