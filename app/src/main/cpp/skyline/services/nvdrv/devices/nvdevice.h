// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <kernel/ipc.h>
#include <kernel/types/KEvent.h>

#define NFUNC(function) std::bind(&function, this, std::placeholders::_1)

namespace skyline::service::nvdrv::device {
    /**
     * @brief An enumeration of all the devices that can be opened by nvdrv
     */
    enum class NvDeviceType {
        nvhost_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl
        nvhost_gpu, //!< https://switchbrew.org/wiki/NV_services#Channels
        nvhost_nvdec, //!< https://switchbrew.org/wiki/NV_services#Channels
        nvhost_vic, //!< https://switchbrew.org/wiki/NV_services#Channels
        nvmap, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvmap
        nvdisp_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvdisp-ctrl
        nvdisp_disp0, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvdisp-disp0.2C_.2Fdev.2Fnvdisp-disp1
        nvdisp_disp1, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvdisp-disp0.2C_.2Fdev.2Fnvdisp-disp1
        nvcec_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvcec-ctrl
        nvhdcp_up_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhdcp_up-ctrl
        nvdcutil_disp0, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvdcutil-disp0.2C_.2Fdev.2Fnvdcutil-disp1
        nvdcutil_disp1, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvdcutil-disp0.2C_.2Fdev.2Fnvdcutil-disp1
        nvsched_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvsched-ctrl
        nverpt_ctrl, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnverpt-ctrl
        nvhost_as_gpu, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-as-gpu
        nvhost_dbg_gpu, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-dbg-gpu
        nvhost_prof_gpu, //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-prof-gpu
        nvhost_ctrl_gpu //!< https://switchbrew.org/wiki/NV_services#.2Fdev.2Fnvhost-ctrl-gpu
    };

    /**
     * @brief A mapping from a device's path to it's nvDevice entry
     */
    const static std::unordered_map<std::string, NvDeviceType> nvDeviceMap{
        {"/dev/nvhost-ctrl", NvDeviceType::nvhost_ctrl},
        {"/dev/nvhost-gpu", NvDeviceType::nvhost_gpu},
        {"/dev/nvhost-nvdec", NvDeviceType::nvhost_nvdec},
        {"/dev/nvhost-vic", NvDeviceType::nvhost_vic},
        {"/dev/nvmap", NvDeviceType::nvmap},
        {"/dev/nvdisp-ctrl", NvDeviceType::nvdisp_ctrl},
        {"/dev/nvdisp-disp0", NvDeviceType::nvdisp_disp0},
        {"/dev/nvdisp-disp1", NvDeviceType::nvdisp_disp1},
        {"/dev/nvcec-ctrl", NvDeviceType::nvcec_ctrl},
        {"/dev/nvhdcp_up-ctrl", NvDeviceType::nvhdcp_up_ctrl},
        {"/dev/nvdcutil-disp0", NvDeviceType::nvdcutil_disp0},
        {"/dev/nvdcutil-disp1", NvDeviceType::nvdcutil_disp1},
        {"/dev/nvsched-ctrl", NvDeviceType::nvsched_ctrl},
        {"/dev/nverpt-ctrl", NvDeviceType::nverpt_ctrl},
        {"/dev/nvhost-as-gpu", NvDeviceType::nvhost_as_gpu},
        {"/dev/nvhost-dbg-gpu", NvDeviceType::nvhost_dbg_gpu},
        {"/dev/nvhost-prof-gpu", NvDeviceType::nvhost_prof_gpu},
        {"/dev/nvhost-ctrl-gpu", NvDeviceType::nvhost_ctrl_gpu},
    };

    /**
     * @brief This enumerates all the possible error codes returned by the Nvidia driver (https://switchbrew.org/wiki/NV_services#Errors)
     */
    enum NvStatus : u32 {
        Success = 0x0, //!< The operation has succeeded
        NotImplemented = 0x1, //!< The operation is not implemented
        NotSupported = 0x2, //!< The operation is not supported
        NotInitialized = 0x3, //!< The operation uses an uninitialized object
        BadParameter = 0x4, //!< The operation was provided a bad parameter
        Timeout = 0x5, //!< The operation has timed out
        InsufficientMemory = 0x6, //!< The device ran out of memory during the operation
        ReadOnlyAttribute = 0x7, //!< The mutating operation was performed on a read only section
        InvalidState = 0x8, //!< The state of the device was invalid
        InvalidAddress = 0x9, //!< The provided address is invalid
        InvalidSize = 0xA, //!< The provided size is invalid
        BadValue = 0xB, //!< The operation was provided a bad value
        AlreadyAllocated = 0xD, //!< An object was tried to be reallocated
        Busy = 0xE, //!< The device is busy
        ResourceError = 0xF, //!< There was an error accessing the resource
        CountMismatch = 0x10, //!< ?
        SharedMemoryTooSmall = 0x1000, //!< The shared memory segment is too small
        FileOperationFailed = 0x30003, //!< The file operation has failed
        DirOperationFailed = 0x30004, //!< The directory operation has failed
        IoctlFailed = 0x3000F, //!< The IOCTL operation has failed
        AccessDenied = 0x30010, //!< The access to a resource was denied
        FileNotFound = 0x30013, //!< A file was not found
        ModuleNotPresent = 0xA000E, //!< A module was not present
    };

    /**
     * @brief This holds all the input and output data for an IOCTL function
     */
    struct IoctlData {
        std::vector<kernel::ipc::InputBuffer> input; //!< A vector of all input IOCTL buffers
        std::vector<kernel::ipc::OutputBuffer> output; //!< A vector of all output IOCTL buffers
        NvStatus status{NvStatus::Success}; //!< The error code that is returned to the application

        /**
         * @brief This constructor takes 1 input buffer and 1 output buffer, it's used for Ioctl
         * @param input An input buffer
         * @param output An output buffer
         */
        IoctlData(kernel::ipc::InputBuffer input, kernel::ipc::OutputBuffer output) : input({input}), output({output}) {}

        /**
         * @brief This constructor takes 1 input buffer, it's used for Ioctl sometimes
         * @param output An output buffer
         */
        IoctlData(kernel::ipc::InputBuffer input) : input({input}) {}

        /**
         * @brief This constructor takes 1 output buffer, it's used for Ioctl sometimes
         * @param output An output buffer
         */
        IoctlData(kernel::ipc::OutputBuffer output) : output({output}) {}

        /**
         * @brief This constructor takes 2 input buffers and 1 output buffer, it's used for Ioctl1
         * @param input1 The first input buffer
         * @param input2 The second input buffer
         * @param output An output buffer
         */
        IoctlData(kernel::ipc::InputBuffer input1, kernel::ipc::InputBuffer input2, kernel::ipc::OutputBuffer output) : input({input1, input2}), output({output}) {}

        /**
         * @brief This constructor takes 1 input buffer and 2 output buffers, it's used for Ioctl2
         * @param input An input buffer
         * @param output1 The first output buffer
         * @param output2 The second output buffer
         */
        IoctlData(kernel::ipc::InputBuffer input, kernel::ipc::OutputBuffer output1, kernel::ipc::OutputBuffer output2) : input({input}), output({output1, output2}) {}
    };

    /**
     * @brief NvDevice is the base class all /dev/nv* devices inherit from
     */
    class NvDevice {
      protected:
        const DeviceState &state; //!< The state of the device
        std::unordered_map<u32, std::function<void(IoctlData &)>> vTable; //!< This holds the mapping from an Ioctl to the actual function

      public:
        u16 refCount{1}; //!< The amount of handles to the device
        NvDeviceType deviceType; //!< The type of the device

        /**
         * @param state The state of the device
         * @param deviceType The type of the device
         * @param vTable The functions in this device
         */
        NvDevice(const DeviceState &state, NvDeviceType deviceType, std::unordered_map<u32, std::function<void(IoctlData &)>> vTable) : state(state), deviceType(deviceType), vTable(vTable) {}

        virtual ~NvDevice() = default;

        /**
         * @brief This returns the name of the current service
         * @note It may not return the exact name the service was initialized with if there are multiple entries in ServiceString
         * @return The name of the service
         */
        std::string getName() {
            std::string serviceName;
            for (const auto&[name, type] : nvDeviceMap)
                if (type == deviceType)
                    serviceName = name;
            return serviceName;
        }

        /**
         * @brief This handles IOCTL calls for devices
         * @param cmd The IOCTL command that was called
         * @param input The input to the IOCTL call
         */
        void HandleIoctl(u32 cmd, IoctlData &input) {
            std::function<void(IoctlData &)> function;
            try {
                function = vTable.at(cmd);
            } catch (std::out_of_range &) {
                state.logger->Warn("Cannot find IOCTL for device '{}': 0x{:X}", getName(), cmd);
                input.status = NvStatus::NotImplemented;
                return;
            }
            try {
                function(input);
            } catch (std::exception &e) {
                throw exception("{} (Device: {})", e.what(), getName());
            }
        }

        virtual std::shared_ptr<kernel::type::KEvent> QueryEvent(u32 eventId) {
            return nullptr;
        }
    };
}
