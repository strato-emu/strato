// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2011 The Android Open Source Project

#pragma once

#include <common/macros.h>

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

    ENUM_STRING(NativeWindowApi, {
        ENUM_CASE(None);
        ENUM_CASE(EGL);
        ENUM_CASE(CPU);
        ENUM_CASE(Media);
        ENUM_CASE(Camera);
    });

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

    ENUM_STRING(NativeWindowTransform, {
        ENUM_CASE(Identity);
        ENUM_CASE(MirrorHorizontal);
        ENUM_CASE(MirrorVertical);
        ENUM_CASE(Rotate90);
        ENUM_CASE(Rotate180);
        ENUM_CASE(Rotate270);
        ENUM_CASE(MirrorHorizontalRotate90);
        ENUM_CASE(MirrorVerticalRotate90);
        ENUM_CASE(InvertDisplay);
    });

    /**
     * @url https://cs.android.com/android/platform/superproject/+/android-5.1.1_r38:system/core/include/system/window.h;l=338-354
     */
    enum class NativeWindowScalingMode : u32 {
        Freeze = 0,
        ScaleToWindow = 1,
        ScaleCrop = 2,
        NoScaleCrop = 3,
    };

    ENUM_STRING(NativeWindowScalingMode, {
        ENUM_CASE(Freeze);
        ENUM_CASE(ScaleToWindow);
        ENUM_CASE(ScaleCrop);
        ENUM_CASE(NoScaleCrop);
    });

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

    ENUM_STRING(NativeWindowQuery, {
        ENUM_CASE(Width);
        ENUM_CASE(Height);
        ENUM_CASE(Format);
        ENUM_CASE(MinUndequeuedBuffers);
        ENUM_CASE(QueuesToWindowComposer);
        ENUM_CASE(ConcreteType);
        ENUM_CASE(DefaultWidth);
        ENUM_CASE(DefaultHeight);
        ENUM_CASE(TransformHint);
        ENUM_CASE(ConsumerRunningBehind);
        ENUM_CASE(ConsumerUsageBits);
        ENUM_CASE(StickyTransform);
        ENUM_CASE(MaxBufferCount);
    });
}
