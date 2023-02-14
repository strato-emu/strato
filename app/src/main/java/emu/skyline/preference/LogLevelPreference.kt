/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.util.AttributeSet
import androidx.preference.R
import emu.skyline.settings.NativeSettings

class LogLevelPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.dialogPreferenceStyle) : IntegerListPreference(context, attrs, defStyleAttr) {
    init {
        setOnPreferenceChangeListener { _, newValue ->
            NativeSettings.setLogLevel(newValue as Int)
            true
        }
    }
}
