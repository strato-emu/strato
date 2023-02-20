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
import kotlin.reflect.KMutableProperty
import kotlin.reflect.full.memberProperties

/**
 * Settings used during emulation. Use [forTitleId] to retrieve settings for a given `titleId`.
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

    // Audio
    var isAudioOutputDisabled by sharedPreferences(context, false, prefName = prefName)

    // Display
    var perfStats by sharedPreferences(context, false, prefName = prefName)
    var maxRefreshRate by sharedPreferences(context, false, prefName = prefName)
    var orientation by sharedPreferences(context, ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE, prefName = prefName)
    var aspectRatio by sharedPreferences(context, 0, prefName = prefName)
    var respectDisplayCutout by sharedPreferences(context, false, prefName = prefName)

    // GPU
    var gpuDriver by sharedPreferences(context, SYSTEM_GPU_DRIVER, prefName = prefName)
    var forceTripleBuffering by sharedPreferences(context, true, prefName = prefName)
    var disableFrameThrottling by sharedPreferences(context, false, prefName = prefName)
    var executorSlotCountScale by sharedPreferences(context, 6, prefName = prefName)
    var executorFlushThreshold by sharedPreferences(context, 256, prefName = prefName)
    var useDirectMemoryImport by sharedPreferences(context, false, prefName = prefName)
    var forceMaxGpuClocks by sharedPreferences(context, false, prefName = prefName)
    var freeGuestTextureMemory by sharedPreferences(context, true, prefName = prefName)
    var disableShaderCache by sharedPreferences(context, false, prefName = prefName)

    // Hacks
    var enableFastGpuReadbackHack by sharedPreferences(context, false, prefName = prefName)
    var enableFastReadbackWrites by sharedPreferences(context, false, prefName = prefName)
    var disableSubgroupShuffle by sharedPreferences(context, false, prefName = prefName)

    // Debug
    var validationLayer by sharedPreferences(context, false, prefName = prefName)

    /**
     * Copies all settings from the global settings to this instance.
     * This is a no-op if the instance this is called on is the global one
     */
    fun copyFromGlobal() {
        if (isGlobal)
            return

        for (prop in EmulationSettings::class.memberProperties) {
            if (prop.name == "useCustomSettings")
                continue

            if (prop is KMutableProperty<*>)
                prop.setter.call(this, prop.get(global))
        }
    }

    companion object {
        const val SYSTEM_GPU_DRIVER = "system"

        /**
         * The global emulation settings instance
         */
        val global by lazy { EmulationSettings(SkylineApplication.context, null) }

        fun prefNameForTitle(titleId : String) = SkylineApplication.context.packageName + "_" + titleId

        /**
         * Returns emulation settings for the given `titleId`
         */
        fun forTitleId(titleId : String) = EmulationSettings(SkylineApplication.context, prefNameForTitle(titleId))

        /**
         * Returns emulation settings for the given preferences name
         */
        fun forPrefName(prefName : String) = EmulationSettings(SkylineApplication.context, prefName)

        /**
         * Returns emulation settings to be used during emulation of the given `titleId`.
         * Global settings are returned if custom settings are disabled for this title
         */
        fun forEmulation(titleId : String) : EmulationSettings {
            var emulationSettings = forTitleId(titleId)

            if (!emulationSettings.useCustomSettings)
                emulationSettings = global

            return emulationSettings
        }
    }
}
