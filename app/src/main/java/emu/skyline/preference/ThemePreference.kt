/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.app.AppCompatDelegate
import androidx.preference.ListPreference

/**
 * This preference is used to set the theme to Light/Dark mode
 */
class ThemePreference : ListPreference {
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(context, attrs, defStyleAttr)

    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs)

    constructor(context: Context?) : super(context)

    /**
     * This changes [AppCompatDelegate.sDefaultNightMode] based on what the user's selection is
     */
    override fun callChangeListener(newValue: Any?): Boolean {
        AppCompatDelegate.setDefaultNightMode(when ((newValue as String).toInt()) {
            0 -> AppCompatDelegate.MODE_NIGHT_NO
            1 -> AppCompatDelegate.MODE_NIGHT_YES
            2 -> AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM
            else -> AppCompatDelegate.MODE_NIGHT_UNSPECIFIED
        })

        return super.callChangeListener(newValue)
    }
}
