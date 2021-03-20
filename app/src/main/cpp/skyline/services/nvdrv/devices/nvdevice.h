// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <kernel/ipc.h>
#include <kernel/types/KEvent.h>

#define NV_STRINGIFY(string) #string
#define NVFUNC(id, Class, Function) std::pair<u32, std::pair<NvStatus(Class::*)(IoctlType, span<u8>, span<u8>), const char*>>{id, {&Class::Function, NV_STRINGIFY(Class::Function)}}
#define NVDEVICE_DECL_AUTO(name, value) decltype(value) name = value
#define NVDEVICE_DECL(...)                                                                \
NVDEVICE_DECL_AUTO(functions, frz::make_unordered_map({__VA_ARGS__}));                    \
NvDeviceFunctionDescriptor GetIoctlFunction(u32 id) override {                            \
    auto& function{functions.at(id)};                                                     \
    return NvDeviceFunctionDescriptor{                                                    \
        reinterpret_cast<DerivedDevice*>(this),                                           \
        reinterpret_cast<decltype(NvDeviceFunctionDescriptor::function)>(function.first), \
        function.second                                                                   \
    };                                                                                    \
}

namespace skyline::service::nvdrv::device {
    using namespace kernel;

    /**
     * @brief All the possible error codes returned by the Nvidia driver
     * @url https://switchbrew.org/wiki/NV_services#Errors
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
     * @brief The IOCTL call variants, they have different buffer configurations
     */
    enum class IoctlType : u8 {
        Ioctl,  //!< 1 input/output buffer
        Ioctl2, //!< 1 input/output buffer + 1 input buffer
        Ioctl3, //!< 1 input/output buffer + 1 output buffer
    };

    /**
     * @brief NvDevice is the base class that all /dev/nv* devices inherit from
     */
    class NvDevice {
      private:
        std::string name; //!< The name of the device

      protected:
        const DeviceState &state;

        class DerivedDevice; //!< A placeholder derived class which is used for class function semantics

        /**
         * @brief A per-function descriptor for NvDevice functions
         */
        struct NvDeviceFunctionDescriptor {
            DerivedDevice *clazz; //!< A pointer to the class that this was derived from, it's used as the 'this' pointer for the function
            NvStatus (DerivedDevice::*function)(IoctlType, span<u8>, span<u8>); //!< A function pointer to the implementation of the function
            const char *name; //!< A pointer to a static string in the format "Class::Function" for the specific device class/function

            constexpr NvStatus operator()(IoctlType type, span<u8> buffer, span<u8> inlineBuffer) {
                return (clazz->*function)(type, buffer, inlineBuffer);
            }
        };

      public:
        NvDevice(const DeviceState &state) : state(state) {}

        virtual ~NvDevice() = default;

        virtual NvDeviceFunctionDescriptor GetIoctlFunction(u32 id) = 0;

        /**
         * @return The name of the class
         * @note The lifetime of the returned string is tied to that of the class
         */
        const std::string &GetName();

        /**
         * @brief Handles IOCTL calls for devices
         * @param cmd The IOCTL command that was called
         */
        NvStatus HandleIoctl(u32 cmd, IoctlType type, span<u8> buffer, span<u8> inlineBuffer);

        virtual std::shared_ptr<kernel::type::KEvent> QueryEvent(u32 eventId) {
            return nullptr;
        }
    };
}
