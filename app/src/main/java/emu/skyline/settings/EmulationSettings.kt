/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.content.Context
import android.content.pm.ActivityInfo
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.utils.sharedPreferences

/**
 * Settings used during emulation.
 */
class EmulationSettings private constructor(context : Context, prefName : String?) {
    // Whether this is the global settings
    val isGlobal = prefName == null

    // Custom settings toggle, should be ignored for global settings
    var useCustomSettings by sharedPreferences(context, false, prefName = prefName)

    // System
    var isDocked by sharedPreferences(context, true, prefName = prefName)
    var usernameValue by sharedPreferences(context, context.getString(R.string.username_default), prefName = prefName)
    var profilePictureValue by sharedPreferences(context, "", prefName = prefName)
    var systemLanguage by sharedPreferences(context, 1, prefName = prefName)
    var systemRegion by sharedPreferences(context, -1, prefName = prefName)

    // Display
    var forceTripleBuffering by sharedPreferences(context, true, prefName = prefName)
    var disableFrameThrottling by sharedPreferences(context, false, prefName = prefName)
    var maxRefreshRate by sharedPreferences(context, false, prefName = prefName)
    var aspectRatio by sharedPreferences(context, 0, prefName = prefName)
    var orientation by sharedPreferences(context, ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE, prefName = prefName)
    var respectDisplayCutout by sharedPreferences(context, false, prefName = prefName)
    var disableShaderCache by sharedPreferences(context, false, prefName = prefName)

    // GPU
    var gpuDriver by sharedPreferences(context, SYSTEM_GPU_DRIVER, prefName = prefName)
    var executorSlotCountScale by sharedPreferences(context, 6, prefName = prefName)
    var executorFlushThreshold by sharedPreferences(context, 256, prefName = prefName)
    var useDirectMemoryImport by sharedPreferences(context, false, prefName = prefName)
    var forceMaxGpuClocks by sharedPreferences(context, false, prefName = prefName)

    // Hacks
    var enableFastGpuReadbackHack by sharedPreferences(context, false, prefName = prefName)
    var enableFastReadbackWrites by sharedPreferences(context, false, prefName = prefName)
    var disableSubgroupShuffle by sharedPreferences(context, false, prefName = prefName)

    // Audio
    var isAudioOutputDisabled by sharedPreferences(context, false, prefName = prefName)

    // Debug
    var validationLayer by sharedPreferences(context, false, prefName = prefName)

    companion object {
        const val SYSTEM_GPU_DRIVER = "system"

        /**
         * The global emulation settings instance
         */
        val global by lazy { EmulationSettings(SkylineApplication.context, null) }
    }
}
