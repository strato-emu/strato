/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context
import android.content.pm.ActivityInfo
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton
import emu.skyline.R

@Singleton
class PreferenceSettings @Inject constructor(@ApplicationContext private val context : Context) {
    // Emulator
    var searchLocation by sharedPreferences(context, "")
    var appTheme by sharedPreferences(context, 2)
    var layoutType by sharedPreferences(context, 1)
    var selectAction by sharedPreferences(context, false)
    var perfStats by sharedPreferences(context, false)
    var logLevel by sharedPreferences(context, 3)

    // System
    var isDocked by sharedPreferences(context, true)
    var usernameValue by sharedPreferences(context, context.getString(R.string.username_default))
    var systemLanguage by sharedPreferences(context, 1)
    var systemRegion by sharedPreferences(context, -1)

    // Display
    var forceTripleBuffering by sharedPreferences(context, true)
    var disableFrameThrottling by sharedPreferences(context, false)
    var maxRefreshRate by sharedPreferences(context, false)
    var aspectRatio by sharedPreferences(context, 0)
    var orientation by sharedPreferences(context, ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE)
    var respectDisplayCutout by sharedPreferences(context, false)

    // GPU
    var gpuDriver by sharedPreferences(context, SYSTEM_GPU_DRIVER)
    var executorSlotCountScale by sharedPreferences(context, 6)

    // Hacks
    var enableFastGpuReadbackHack by sharedPreferences(context, false)

    // Audio
    var isAudioOutputDisabled by sharedPreferences(context, false)

    // Debug
    var validationLayer by sharedPreferences(context, false)

    // Input
    var onScreenControl by sharedPreferences(context, true)
    var onScreenControlRecenterSticks by sharedPreferences(context, true)

    // Other
    var romFormatFilter by sharedPreferences(context, 0)
    var refreshRequired by sharedPreferences(context, false)

    companion object {
        const val SYSTEM_GPU_DRIVER = "system"
    }
}
