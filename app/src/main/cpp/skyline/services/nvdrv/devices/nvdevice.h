// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <cxxabi.h>
#include <common.h>
#include <kernel/ipc.h>
#include <kernel/types/KEvent.h>

#define NVFUNC(id, Class, Function) std::pair<u32, std::function<void(Class*, IoctlData &)>>(id, &Class::Function)
#define NVDEVICE_DECL_AUTO(name, value) decltype(value) name = value
#define NVDEVICE_DECL(...)                                                                                              \
NVDEVICE_DECL_AUTO(functions, frz::make_unordered_map({__VA_ARGS__}));                                                  \
std::function<void(IoctlData &)> GetServiceFunction(u32 index) {                                                        \
    return std::bind(functions.at(index), this, std::placeholders::_1);                                                 \
}

namespace skyline::service::nvdrv::device {
    using namespace kernel;

    /**
     * @brief This enumerates all the possible error codes returned by the Nvidia driver (https://switchbrew.org/wiki/NV_services#Errors)
     */
    enum class NvStatus : u32 {
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
     * @brief NvDevice is the base class that all /dev/nv* devices inherit from
     */
    class NvDevice {
      protected:
        const DeviceState &state; //!< The state of the device

      public:
        inline NvDevice(const DeviceState &state) : state(state) {}

        virtual ~NvDevice() = default;

        virtual std::function<void(IoctlData &)> GetServiceFunction(u32 index) = 0;

        /**
         * @return The name of the class
         */
        std::string GetName();

        /**
         * @brief This handles IOCTL calls for devices
         * @param cmd The IOCTL command that was called
         * @param input The input to the IOCTL call
         */
        void HandleIoctl(u32 cmd, IoctlData &input);

        inline virtual std::shared_ptr<kernel::type::KEvent> QueryEvent(u32 eventId) {
            return nullptr;
        }
    };
}
