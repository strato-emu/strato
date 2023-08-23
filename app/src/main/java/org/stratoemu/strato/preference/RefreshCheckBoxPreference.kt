/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.util.AttributeSet
import androidx.preference.CheckBoxPreference
import androidx.preference.R
import org.stratoemu.strato.di.getSettings

/**
 * This preference is used with checkboxes that need to refresh the main activity when changed
 */
class RefreshCheckBoxPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.checkBoxPreferenceStyle) : CheckBoxPreference(context, attrs, defStyleAttr) {
    override fun onClick() {
        super.onClick()
        context.getSettings().refreshRequired = true
    }
}
