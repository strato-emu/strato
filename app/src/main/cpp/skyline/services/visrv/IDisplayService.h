// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <services/base_service.h>
#include <services/serviceman.h>
#include <services/common/parcel.h>

namespace skyline::service::visrv {
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
        IDisplayService(const DeviceState &state, ServiceManager &manager, const Service serviceType, const std::string &serviceName, const std::unordered_map<u32, std::function<void(type::KSession & , ipc::IpcRequest & , ipc::IpcResponse & )>> &vTable);

        /**
         * @brief This creates a stray layer using a display's ID and returns a layer ID and the corresponding buffer ID
         */
        void CreateStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);

        /**
         * @brief This takes a layer ID and destroys the corresponding stray layer
         */
        void DestroyStrayLayer(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
