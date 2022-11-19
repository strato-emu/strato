/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context
import emu.skyline.BuildConfig

/**
 * The settings that will be passed to libskyline when running and executable
 */
class NativeSettings(context : Context, pref : PreferenceSettings) {
    // System
    var isDocked : Boolean = pref.isDocked
    var usernameValue : String = pref.usernameValue
    var systemLanguage : Int = pref.systemLanguage
    var systemRegion : Int = pref.systemRegion

    // Display
    var forceTripleBuffering : Boolean = pref.forceTripleBuffering
    var disableFrameThrottling : Boolean = pref.disableFrameThrottling

    // GPU
    var gpuDriver : String = if (pref.gpuDriver == PreferenceSettings.SYSTEM_GPU_DRIVER) "" else pref.gpuDriver
    var gpuDriverLibraryName : String = if (pref.gpuDriver == PreferenceSettings.SYSTEM_GPU_DRIVER) "" else GpuDriverHelper.getLibraryName(context, pref.gpuDriver)
    var executorSlotCountScale : Int = pref.executorSlotCountScale

    // Hacks
    var enableFastGpuReadbackHack : Boolean = pref.enableFastGpuReadbackHack

    // Audio
    var isAudioOutputDisabled : Boolean = pref.isAudioOutputDisabled

    // Debug
    var validationLayer : Boolean = BuildConfig.BUILD_TYPE != "release" && pref.validationLayer

    /**
     * Updates settings in libskyline during emulation
     */
    external fun updateNative()

    companion object {
        /**
         * Sets libskyline logger level to the given one
         */
        external fun setLogLevel(logLevel : Int)
    }
}
