/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.preference.Preference

/**
 * Some preferences need results from activities, this delegates the results to them
 */
abstract class ActivityResultPreference @JvmOverloads constructor(context : Context?, attrs : AttributeSet? = null, defStyleAttr : Int = 0) : Preference(context, attrs, defStyleAttr) {
    var requestCode = 0

    abstract fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?)
}
