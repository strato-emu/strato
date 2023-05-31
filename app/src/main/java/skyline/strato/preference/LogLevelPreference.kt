/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.preference

import android.content.Context
import android.util.AttributeSet
import androidx.preference.R
import skyline.strato.settings.NativeSettings

class LogLevelPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.dialogPreferenceStyle) : IntegerListPreference(context, attrs, defStyleAttr) {
    init {
        setOnPreferenceChangeListener { _, newValue ->
            NativeSettings.setLogLevel(newValue as Int)
            true
        }
    }
}
