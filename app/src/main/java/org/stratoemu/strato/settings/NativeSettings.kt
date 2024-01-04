/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.settings

import android.content.Context
import org.stratoemu.strato.BuildConfig
import org.stratoemu.strato.utils.GpuDriverHelper
import kotlinx.serialization.Serializable

/**
 * The settings that will be passed to libskyline when running and executable
 */
@Serializable
@Suppress("unused")
data class NativeSettings(
    // System
    var isDocked : Boolean,
    var usernameValue : String,
    var profilePictureValue : String,
    var systemLanguage : Int,
    var systemRegion : Int,
    var isInternetEnabled : Boolean,

    // Audio
    var isAudioOutputDisabled : Boolean,

    // GPU
    var gpuDriver : String,
    var gpuDriverLibraryName : String,
    var forceTripleBuffering : Boolean,
    var disableFrameThrottling : Boolean,
    var executorSlotCountScale : Int,
    var executorFlushThreshold : Int,
    var useDirectMemoryImport : Boolean,
    var forceMaxGpuClocks : Boolean,
    var freeGuestTextureMemory : Boolean,
    var disableShaderCache : Boolean,

    // Hacks
    var enableFastGpuReadbackHack : Boolean,
    var enableFastReadbackWrites : Boolean,
    var disableSubgroupShuffle : Boolean,

    // Debug
    var logLevel : Int,
    var validationLayer : Boolean
) {
    constructor(context : Context, pref : EmulationSettings) : this(
        pref.isDocked,
        pref.usernameValue,
        pref.profilePictureValue,
        pref.systemLanguage,
        pref.systemRegion,
        pref.isInternetEnabled,
        pref.isAudioOutputDisabled,
        if (pref.gpuDriver == EmulationSettings.SYSTEM_GPU_DRIVER) "" else pref.gpuDriver,
        if (pref.gpuDriver == EmulationSettings.SYSTEM_GPU_DRIVER) "" else GpuDriverHelper.getLibraryName(context, pref.gpuDriver),
        pref.forceTripleBuffering,
        pref.disableFrameThrottling,
        pref.executorSlotCountScale,
        pref.executorFlushThreshold,
        pref.useDirectMemoryImport,
        pref.forceMaxGpuClocks,
        pref.freeGuestTextureMemory,
        pref.disableShaderCache,
        pref.enableFastGpuReadbackHack,
        pref.enableFastReadbackWrites,
        pref.disableSubgroupShuffle,
        pref.logLevel,
        BuildConfig.BUILD_TYPE != "release" && pref.validationLayer
    )

    /**
     * Updates settings in libskyline during emulation
     */
    external fun updateNative()
}
