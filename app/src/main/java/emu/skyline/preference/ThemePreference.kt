/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.app.AppCompatDelegate
import androidx.preference.ListPreference
import androidx.preference.R

/**
 * This preference is used to set the theme to Light/Dark mode
 */
class ThemePreference @JvmOverloads constructor(context : Context?, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.dialogPreferenceStyle) : ListPreference(context, attrs, defStyleAttr) {
    /**
     * This changes [AppCompatDelegate.sDefaultNightMode] based on what the user's selection is
     */
    override fun callChangeListener(newValue : Any?) : Boolean {
        AppCompatDelegate.setDefaultNightMode(when ((newValue as String).toInt()) {
            0 -> AppCompatDelegate.MODE_NIGHT_NO
            1 -> AppCompatDelegate.MODE_NIGHT_YES
            2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
        })

        return super.callChangeListener(newValue)
    }
}
