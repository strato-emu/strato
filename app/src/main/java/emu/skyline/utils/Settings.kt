/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.content.Context

class Settings(context : Context) {
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
}
