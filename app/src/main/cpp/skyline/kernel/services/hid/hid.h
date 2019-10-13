#pragma once

#include <kernel/services/base_service.h>
#include <kernel/services/serviceman.h>
#include <kernel/types/KProcess.h>

namespace skyline::constant {
    constexpr size_t hidSharedMemSize = 0x40000; //!< The size of HID Shared Memory (https://switchbrew.org/wiki/HID_Shared_Memory)
}

namespace skyline::kernel::service::hid {
    /**
     * @brief IAppletResource is used to get the handle to the HID shared memory (https://switchbrew.org/wiki/HID_services#IAppletResource)
     */
    class IAppletResource : public BaseService {
      public:
        IAppletResource(const DeviceState &state, ServiceManager& manager);

        std::shared_ptr<type::KSharedMemory> hidSharedMemory;

        /**
         * @brief This opens a handle to HID shared memory (https://switchbrew.org/wiki/HID_services#GetSharedMemoryHandle)
         */
        void GetSharedMemoryHandle(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };

    /**
     * @brief hid or Human Interface Device service is used to access input devices (https://switchbrew.org/wiki/HID_services#hid)
     */
    class hid : public BaseService {
      private:
        std::shared_ptr<IAppletResource> resource{};
      public:
        hid(const DeviceState &state, ServiceManager& manager);

        /**
         * @brief This returns an IAppletResource (https://switchbrew.org/wiki/HID_services#CreateAppletResource)
         */
        void CreateAppletResource(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response);
    };
}
