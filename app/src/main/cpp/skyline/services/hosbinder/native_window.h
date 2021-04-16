// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2011 The Android Open Source Project

#pragma once

#define ENUM_CASE(name, key) \
    case name::key:          \
    return #key

namespace skyline::service::hosbinder {
    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/window.h;l=300-318
     */
    enum class NativeWindowApi : u32 {
        None = 0,
        EGL = 1, //!< All GPU presentation APIs including EGL, Vulkan and NVN conform to this
        CPU = 2,
        Media = 3,
        Camera = 4,
    };

    constexpr const char *ToString(NativeWindowApi api) {
        switch (api) {
            ENUM_CASE(NativeWindowApi, None);
            ENUM_CASE(NativeWindowApi, EGL);
            ENUM_CASE(NativeWindowApi, CPU);
            ENUM_CASE(NativeWindowApi, Media);
            ENUM_CASE(NativeWindowApi, Camera);
            default:
                return "Unknown";
        }
    }

    /**
     * @note A few combinations of transforms that are not in the NATIVE_WINDOW_TRANSFORM enum were added to assist with conversion to/from Vulkan transforms
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/window.h;l=321-335
     */
    enum class NativeWindowTransform : u32 {
        Identity = 0b0,
        MirrorHorizontal = 0b1,
        MirrorVertical = 0b10,
        Rotate90 = 0b100,
        Rotate180 = MirrorHorizontal | MirrorVertical,
        Rotate270 = Rotate180 | Rotate90,
        MirrorHorizontalRotate90 = MirrorHorizontal | Rotate90,
        MirrorVerticalRotate90 = MirrorVertical | Rotate90,
        InvertDisplay = 0b1000,
    };

    constexpr const char *ToString(NativeWindowTransform transform) {
        switch (transform) {
            ENUM_CASE(NativeWindowTransform, Identity);
            ENUM_CASE(NativeWindowTransform, MirrorHorizontal);
            ENUM_CASE(NativeWindowTransform, MirrorVertical);
            ENUM_CASE(NativeWindowTransform, Rotate90);
            ENUM_CASE(NativeWindowTransform, Rotate180);
            ENUM_CASE(NativeWindowTransform, Rotate270);
            ENUM_CASE(NativeWindowTransform, MirrorHorizontalRotate90);
            ENUM_CASE(NativeWindowTransform, MirrorVerticalRotate90);
            ENUM_CASE(NativeWindowTransform, InvertDisplay);
            default:
                return "Unknown";
        }
    }

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/window.h;l=338-354
     */
    enum class NativeWindowScalingMode : u32 {
        Freeze = 0,
        ScaleToWindow = 1,
        ScaleCrop = 2,
        NoScaleCrop = 3,
    };

    constexpr const char *ToString(NativeWindowScalingMode scalingMode) {
        switch (scalingMode) {
            ENUM_CASE(NativeWindowScalingMode, Freeze);
            ENUM_CASE(NativeWindowScalingMode, ScaleToWindow);
            ENUM_CASE(NativeWindowScalingMode, ScaleCrop);
            ENUM_CASE(NativeWindowScalingMode, NoScaleCrop);
            default:
                return "Unknown";
        }
    }

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/window.h;l=127-265
     */
    enum class NativeWindowQuery : u32 {
        Width = 0,
        Height = 1,
        Format = 2,
        MinUndequeuedBuffers = 3,
        QueuesToWindowComposer = 4,
        ConcreteType = 5,
        DefaultWidth = 6,
        DefaultHeight = 7,
        TransformHint = 8,
        ConsumerRunningBehind = 9,
        ConsumerUsageBits = 10,
        StickyTransform = 11,
        MaxBufferCount = 12, //!< A custom query for HOS which returns the maximum number of buffers that can be allocated at once
    };

    constexpr const char *ToString(NativeWindowQuery query) {
        switch (query) {
            ENUM_CASE(NativeWindowQuery, Width);
            ENUM_CASE(NativeWindowQuery, Height);
            ENUM_CASE(NativeWindowQuery, Format);
            ENUM_CASE(NativeWindowQuery, MinUndequeuedBuffers);
            ENUM_CASE(NativeWindowQuery, QueuesToWindowComposer);
            ENUM_CASE(NativeWindowQuery, ConcreteType);
            ENUM_CASE(NativeWindowQuery, DefaultWidth);
            ENUM_CASE(NativeWindowQuery, DefaultHeight);
            ENUM_CASE(NativeWindowQuery, TransformHint);
            ENUM_CASE(NativeWindowQuery, ConsumerRunningBehind);
            ENUM_CASE(NativeWindowQuery, ConsumerUsageBits);
            ENUM_CASE(NativeWindowQuery, StickyTransform);
            default:
                return "Unknown";
        }
    }
}

#undef ENUM_CASE
