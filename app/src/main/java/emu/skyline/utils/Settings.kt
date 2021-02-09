/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class Settings @Inject constructor(@ApplicationContext private val context : Context) {
    var layoutType by sharedPreferences(context, "1")

    var searchLocation by sharedPreferences(context, "")

    var refreshRequired by sharedPreferences(context, false)

    var appTheme by sharedPreferences(context, "2")

    var selectAction by sharedPreferences(context, false)

    var perfStats by sharedPreferences(context, false)

    var operationMode by sharedPreferences(context, true)

    var onScreenControl by sharedPreferences(context, true)

    var onScreenControlRecenterSticks by sharedPreferences(context, true)

    var logCompact by sharedPreferences(context, false)

    var logLevel by sharedPreferences(context, "3")

    var filter by sharedPreferences(context, 0)
}
