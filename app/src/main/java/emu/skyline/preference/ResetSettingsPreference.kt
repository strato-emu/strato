/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.app.Activity
import android.content.Context
import android.util.AttributeSet
import androidx.preference.Preference
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import emu.skyline.R

/**
 * Clears all values in the current shared preferences, showing a dialog to confirm the action
 * This preference recreates the activity to update the UI after modifying shared preferences
 */
class ResetSettingsPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    init {
        setOnPreferenceClickListener {
            MaterialAlertDialogBuilder(context)
                .setTitle(title)
                .setMessage(R.string.reset_settings_warning)
                .setPositiveButton(android.R.string.ok) { _, _ ->
                    // Clear all shared preferences
                    sharedPreferences?.apply { edit().clear().apply() }
                    // Recreate the activity to update the UI after modifying shared preferences
                    (context as? Activity)?.recreate()
                }
                .setNegativeButton(android.R.string.cancel, null)
                .show()

            true
        }
    }
}
