// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2005 The Android Open Source Project
// Copyright © 2019-2020 Ryujinx Team and Contributors

#pragma once

#include <common.h>
#include <common/macros.h>
#include <soc/host1x.h>
#include <services/common/fence.h>

namespace skyline::service::hosbinder {
    /**
     * @brief An enumeration of all status codes for Android including Binder IPC
     * @note We don't want to depend on POSIX <errno.h> so we just resolve all macros to their numerical values
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/utils/Errors.h
     */
    enum class AndroidStatus : i32 {
        Ok = 0,
        UnknownError = std::numeric_limits<i32>::min(),
        NoMemory = -12,
        InvalidOperation = -38,
        BadValue = -22,
        BadType = UnknownError + 1,
        NameNotFound = -2,
        PermissionDenied = -1,
        NoInit = -19,
        AlreadyExists = -17,
        DeadObject = -32,
        FailedTransaction = UnknownError + 2,
        JParksBrokeIt = -32,
        BadIndex = -75,
        NotEnoughData = -61,
        WouldBlock = -11,
        TimedOut = -110,
        UnknownTransaction = -74,
        FdsNotAllowed = UnknownError + 7,
        Busy = -16, //!< An alias for -EBUSY which is used in BufferQueueProducer
    };

    /**
     * @brief Nvidia and Nintendo's Android fence implementation, this significantly differs from the Android implementation (All FDs are inlined as integers rather than explicitly passed as FDs) but is a direct replacement
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/Fence.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/ui/Fence.cpp
     */
    struct AndroidFence {
        u32 fenceCount{}; //!< The amount of active fences in the array
        std::array<nvdrv::Fence, 4> fences{}; //!< Nvidia's Android fence can hold a maximum of 4 fence FDs

        static constexpr u32 InvalidFenceId{0xFFFFFFFF}; //!< A magic value for the syncpoint ID of invalid fences (https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/Fence.h;l=61)

        /**
         * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/ui/Fence.cpp;l=34-36
         * @note Only initializing the first fence is intentional and matches Nvidia's AndroidFence implementation
         */
        AndroidFence() : fenceCount(0), fences({InvalidFenceId}) {}

        /**
         * @brief Wait on all native fences in this Android fence till they're signalled
         */
        void Wait(soc::host1x::Host1X &host1x) const {
            if (fenceCount > fences.size())
                throw exception("Wait has larger fence count ({}) than storage size ({})", fenceCount, fences.size());
            for (auto it{fences.begin()}, end{fences.begin() + fenceCount}; it < end; it++)
                if (it->id != InvalidFenceId)
                    host1x.syncpoints.at(it->id).Wait(it->value, std::chrono::steady_clock::duration::max());
        }
    };

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/android/rect.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/Rect.h
     * @note We use unsigned values rather than signed as this makes it easier to error check, negative values are not valid in any location we use them in
     */
    struct AndroidRect {
        u32 left;
        u32 top;
        u32 right;
        u32 bottom;

        /**
         * @return If the rectangle had any defined bounds
         */
        constexpr operator bool() {
            return left || top || right || bottom;
        }

        auto operator<=>(const AndroidRect &) const = default;
    };

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/PixelFormat.h;l=35-68
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/graphics.h;l=44-321
     */
    enum class AndroidPixelFormat {
        None = 0,
        Custom = -4,
        Translucent = -3,
        Transparent = -2,
        Opaque = -1,
        RGBA8888 = 1,   //! 4x8-bit RGBA
        RGBX8888 = 2,   //! 4x8-bit RGB0
        RGB888 = 3,     //! 3x8-bit RGB
        RGB565 = 4,     //!  16-bit RGB
        BGRA8888 = 5,   //! 4x8-bit BGRA
        RGBA5551 = 6,   //!  16-bit ARGB
        RGBA4444 = 7,   //!  16-bit ARGB
        sRGBA8888 = 12, //! 4x8-bit sRGB + A
        sRGBX8888 = 13, //! 4x8-bit sRGB + 0
    };

    ENUM_STRING(AndroidPixelFormat, {
        ENUM_CASE(None);
        ENUM_CASE(Custom);
        ENUM_CASE(Translucent);
        ENUM_CASE(Transparent);
        ENUM_CASE(Opaque);
        ENUM_CASE(RGBA8888);
        ENUM_CASE(RGBX8888);
        ENUM_CASE(RGB888);
        ENUM_CASE(RGB565);
        ENUM_CASE(BGRA8888);
        ENUM_CASE(RGBA5551);
        ENUM_CASE(RGBA4444);
        ENUM_CASE(sRGBA8888);
        ENUM_CASE(sRGBX8888);
    })

    /**
     * @brief The layout of the surface's pixels in GPU memory
     */
    enum class NvSurfaceLayout : u32 {
        Pitch = 0x1, //!< A linear pixel arrangement but rows aligned to the pitch
        Tiled = 0x2, //!< A legacy 16Bx16 block layout which was used in NVENC prior to being deprecated
        Blocklinear = 0x3, //!< A generic block layout which is further defined by it's kind
    };

    ENUM_STRING(NvSurfaceLayout, {
        ENUM_CASE(Pitch);
        ENUM_CASE(Tiled);
        ENUM_CASE(Blocklinear);
    })

    /**
     * @brief The kind of tiling used to arrange pixels in a blocklinear surface
     */
    enum class NvKind : u32 {
        Pitch = 0x0,
        Generic16Bx2 = 0xFE, //!< A block layout with sector width of 16 and sector height as 2 (16Bx2)
        Invalid = 0xFF,
    };

    /**
     * @brief The format in which the surface is scanned out to a display
     */
    enum class NvDisplayScanFormat : u32 {
        Progressive, //!< All rows of the image are updated at once
        Interlaced, //!< Odd and even rows are updated in an alternating pattern
    };

    ENUM_STRING(NvDisplayScanFormat, {
        ENUM_CASE(Progressive);
        ENUM_CASE(Interlaced);
    })

    #pragma pack(push, 1)

    /**
     * @brief All metadata about a single surface, most of this will mirror the data in NvGraphicHandle and GraphicBuffer
     */
    struct NvSurface {
        u32 width;
        u32 height;
        u64 format; //!< The internal format of the surface
        NvSurfaceLayout layout;
        u32 pitch; //!< The pitch of the surface for pitch-linear surfaces
        u32 nvmapHandle; //!< The handle of the buffer containing this surface in regards to /dev/nvmap
        u32 offset; //!< The offset of the surface into the buffer
        NvKind kind;
        u32 blockHeightLog2; //!< The log2 of the block height in blocklinear surfaces
        NvDisplayScanFormat scanFormat;
        u32 oddRowOffset; //!< The offset of all odd rows relative to the start of the buffer
        u64 flags;
        u64 size;
        u32 _unk_[6];
    };
    static_assert(sizeof(NvSurface) == 0x58);

    /**
     * @brief The integers of the native_handle used by Nvidia to marshall the surfaces in this buffer
     */
    struct NvGraphicHandle {
        constexpr static u32 Magic{0xDAFFCAFF};
        u32 _unk0_; //!< This is presumably a file descriptor that Nintendo removed as it's value is always a null FD (-1)
        u32 nvmapId; //!< The ID of the buffer in regards to /dev/nvmap
        u32 _unk1_;
        u32 magic; //!< The magic for the buffer (0xDAFFCAFF)
        u32 ownerPid; //!< Same as the upper 32-bits of the ID in the GraphicBuffer (0x2F)
        u32 type;
        u32 usage; //!< The Gralloc usage flags, same as GraphicBuffer
        u32 format; //!< The internal format of the buffer
        u32 externalFormat; //!< The external format that's exposed by Gralloc
        u32 stride;
        u32 size; //!< The size of the buffer in bytes
        u32 surfaceCount; //!< The amount of valid surfaces in the array
        u32 _unk2_;
        std::array<NvSurface, 3> surfaces;
        u32 _unk3_[2];
    };
    static_assert(sizeof(NvGraphicHandle) == 0x144);

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/include/ui/GraphicBuffer.h
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:frameworks/native/libs/ui/GraphicBuffer.cpp;l=266-301
     */
    struct GraphicBuffer {
        constexpr static u32 Magic{'GBFR'}; //!< The magic is in little-endian, we do not need to use 'util::MakeMagic'
        u32 magic; //!< The magic of the Graphics BuFfeR: 'GBFR' (0x47424652)
        u32 width;
        u32 height;
        u32 stride;
        AndroidPixelFormat format;
        u32 usage; //!< The Gralloc usage flags for the buffer, this is a deprecated 32-bit usage flag
        u64 id; //!< A 64-bit ID composed of a 32-bit PID and 32-bit incrementing counter
        u32 fdCount; //!< The amount of FDs being transferred alongside this buffer, NN uses none so this should be 0
        u32 intCount; //!< The size of the native buffer in 32-bit integer units, should be equal to the size of NvNativeHandle in 32-bit units
        NvGraphicHandle graphicHandle;
    };
    static_assert(sizeof(GraphicBuffer) == 0x16C);

    #pragma pack(pop)
}
