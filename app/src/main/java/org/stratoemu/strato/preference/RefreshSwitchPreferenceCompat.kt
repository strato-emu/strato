/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.util.AttributeSet
import androidx.preference.R
import androidx.preference.SwitchPreferenceCompat
import org.stratoemu.strato.di.getSettings

/**
 * This preference is used with switches that need to refresh the main activity when changed
 */
class RefreshSwitchPreferenceCompat @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.switchPreferenceCompatStyle) : SwitchPreferenceCompat(context, attrs, defStyleAttr) {
    override fun onClick() {
        super.onClick()
        context.getSettings().refreshRequired = true
    }
}
