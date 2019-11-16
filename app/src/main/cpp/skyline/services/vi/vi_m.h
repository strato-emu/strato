#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <gpu.h>
#include <gpu/parcel.h>

namespace skyline::service::vi {
    /**
     * @brief This service is used to get an handle to #IApplicationDisplayService (https://switchbrew.org/wiki/Display_services#vi:m)
     */
    class vi_m : public BaseService {
      public:
        vi_m(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief This returns an handle to #IApplicationDisplayService (https://switchbrew.org/wiki/Display_services#GetDisplayService)
         */
        void GetDisplayService(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief This is the base class for all IDisplayService variants with common functions
     */
    class IDisplayService : public BaseService {
      protected:
        /**
         * @brief This is the format of the parcel used in OpenLayer/CreateStrayLayer
         */
        struct LayerParcel {
            u32 type; //!< The type of the layer
            u32 pid; //!< The PID that the layer belongs to
            u32 bufferId; //!< The buffer ID of the layer
            u32 _pad0_[3];
            u8 string[0x8]; //!< "dispdrv"
            u64 _pad1_;
        };
        static_assert(sizeof(LayerParcel) == 0x28);

      public:
        IDisplayService(const DeviceState &state, ServiceManager &manager, Service serviceType, const std::unordered_map<u32, std::function<void(type::KSession &, ipc::IpcRequest &, ipc::IpcResponse &)>> &vTable);

        /**
         * @brief This takes a display's ID and returns a layer ID and the corresponding buffer ID
         */
        void CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

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

    /**
     * @brief This service retrieves information about a display in context of the entire system (https://switchbrew.org/wiki/Display_services#ISystemDisplayService)
     */
    class ISystemDisplayService : public IDisplayService {
      public:
        ISystemDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Sets the Z index of a layer
         */
        void SetLayerZ(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief This service retrieves information about a display in context of the entire system (https://switchbrew.org/wiki/Display_services#IManagerDisplayService)
     */
    class IManagerDisplayService : public IDisplayService {
      public:
        IManagerDisplayService(const DeviceState &state, ServiceManager &manager);

        /**
         * @brief Creates a managed layer on a specific display
         */
        void CreateManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Destroys a managed layer created on a specific display
         */
        void DestroyManagedLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This takes a layer's ID and adds it to the layer stack
         */
        void AddToLayerStack(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
