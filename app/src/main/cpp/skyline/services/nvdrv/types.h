// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>

namespace skyline::service::nvdrv {
    using FileDescriptor = i32;
    constexpr FileDescriptor InvalidFileDescriptor{-1};

    struct SessionPermissions {
        bool AccessGpu;
        bool AccessGpuDebug;
        bool AccessGpuSchedule;
        bool AccessVic;
        bool AccessVideoEncoder;
        bool AccessVideoDecoder;
        bool AccessTsec;
        bool AccessJpeg;
        bool AccessDisplay;
        bool AccessImportMemory;
        bool NoCheckedAruid;
        bool ModifyGraphicsMargin;
        bool DuplicateNvMapHandles;
        bool ExportNvMapHandles;
    };

    struct SessionContext {
        SessionPermissions perms;
        bool internalSession;
    };

    /**
     * @brief Permissions that applications have when using the 'nvdrv' service
     */
    static constexpr SessionPermissions ApplicationSessionPermissions {
        .AccessGpu = true,
        .AccessGpuDebug = true,
        .AccessVic = true,
        .AccessVideoDecoder = true,
        .ModifyGraphicsMargin = true
    };

    /**
     * @brief A bitfield struct that unpacks an ioctl number, used as an alternative to Linux's macros
     */
    union IoctlDescriptor {
        struct {
            u8 function; //!< The function number corresponding to a specific call in the driver
            i8 magic; //!< Unique to each driver
            u16 size : 14; //!< Size of the argument buffer
            bool in : 1; //!< Guest is writing, we are reading
            bool out : 1; //!< Guest is reading, we are writing
        };

        u32 raw;
    };
    static_assert(sizeof(IoctlDescriptor) == sizeof(u32));

    /**
     * @brief NvRm result codes that are translated from the POSIX error codes used internally
     * @url https://switchbrew.org/wiki/NV_services#NvError
     */
    enum class NvResult : i32 {
        Success = 0x0,
        NotImplemented = 0x1,
        NotSupported = 0x2,
        NotInitialized = 0x3,
        BadParameter = 0x4,
        Timeout = 0x5,
        InsufficientMemory = 0x6,
        ReadOnlyAttribute = 0x7,
        InvalidState = 0x8,
        InvalidAddress = 0x9,
        InvalidSize = 0xA,
        BadValue = 0xB,
        AlreadyAllocated = 0xD,
        Busy = 0xE,
        ResourceError = 0xF,
        CountMismatch = 0x10,
        Overflow = 0x11,
        FileOperationFailed = 0x30003,
        AccessDenied = 0x30010,
        IoctlFailed = 0x3000F
    };

    template<typename ValueType>
    using NvResultValue = ResultValue<ValueType, NvResult>;
}
