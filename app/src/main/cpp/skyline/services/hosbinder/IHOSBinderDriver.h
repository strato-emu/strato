// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include "GraphicBufferProducer.h"

namespace skyline::service::hosbinder {
    /**
     * @brief A display identifier specific to HOS, translated to a corresponding Android display internally
     * @url https://switchbrew.org/wiki/Display_services#DisplayName
     */
    enum class DisplayId : u64 {
        Default, //!< Automatically determines the default display
        External, //!< Refers to an external display, if any
        Edid, //!< Refers to an external display with EDID capabilities
        Internal, //!< Refers to the internal display on the Switch
        Null, //!< A placeholder display which doesn't refer to any display
    };

    ENUM_STRING(DisplayId, {
        ENUM_CASE(Default);
        ENUM_CASE(External);
        ENUM_CASE(Edid);
        ENUM_CASE(Internal);
        ENUM_CASE(Null);
    })

    /**
     * @brief nvnflinger:dispdrv or nns::hosbinder::IHOSBinderDriver is a translation layer between Android Binder IPC and HOS IPC to communicate with the Android display stack
     */
    class IHOSBinderDriver : public BaseService {
      private:
        DisplayId displayId{DisplayId::Null}; //!< The ID of the display that the layer is connected to

        constexpr static i32 InitialStrongReferenceCount{std::numeric_limits<i32>::min()}; //!< Initial value for the strong reference count, weak references will keep the object alive till the strong reference count is first mutated
        i32 layerStrongReferenceCount; //!< The amount of strong references to the layer object
        i32 layerWeakReferenceCount; //!< The amount of weak references to the layer object, these only matter when there are no strong references

        constexpr static u64 DefaultLayerId{1}; //!< The VI ID of the default (and only) layer in our surface stack
        constexpr static u32 DefaultBinderLayerHandle{1}; //!< The handle as assigned by SurfaceFlinger of the default layer
        std::shared_ptr<GraphicBufferProducer> layer{}; //!< The IGraphicBufferProducer backing the layer (NativeWindow)

        nvdrv::core::NvMap &nvMap;

      public:
        IHOSBinderDriver(const DeviceState &state, ServiceManager &manager, nvdrv::core::NvMap &nvMap);

        /**
         * @brief Emulates the transaction of parcels between a IGraphicBufferProducer and the application
         * @url https://switchbrew.org/wiki/Nvnflinger_services#TransactParcel
         */
        Result TransactParcel(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Adjusts the reference counts to the underlying Android reference counted object
         * @url https://switchbrew.org/wiki/Nvnflinger_services#AdjustRefcount
         */
        Result AdjustRefcount(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief Adjusts the reference counts to the underlying binder, it's stubbed as we aren't using the real symbols
         * @url https://switchbrew.org/wiki/Nvnflinger_services#GetNativeHandle
         */
        Result GetNativeHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @note This will throw an exception if another display was opened and not closed
         */
        DisplayId OpenDisplay(std::string_view name);

        /**
         * @note This **must** be called prior to opening a different Display
         */
        void CloseDisplay(DisplayId id);

        /**
         * @return An ID that can be utilized to refer to the layer
         * @note This will throw an exception if the specified display has not been opened
         */
        u64 CreateLayer(DisplayId displayId);

        /**
         * @return A parcel with a flattened IBinder to the IGraphicBufferProducer of the layer
         * @note This will throw an exception if the specified display has not been opened
         */
        Parcel OpenLayer(DisplayId displayId, u64 layerId);

        /**
         * @note This **must** be called prior to destroying the layer
         */
        void CloseLayer(u64 layerId);

        /**
         * @note This **must** be called prior to opening a different Display
         */
        void DestroyLayer(u64 layerId);

      SERVICE_DECL(
          SFUNC(0x0, IHOSBinderDriver, TransactParcel),
          SFUNC(0x1, IHOSBinderDriver, AdjustRefcount),
          SFUNC(0x2, IHOSBinderDriver, GetNativeHandle),
          SFUNC(0x3, IHOSBinderDriver, TransactParcel)
      )
    };
}
