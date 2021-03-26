/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import com.google.android.material.snackbar.Snackbar
import emu.skyline.KeyReader
import emu.skyline.R
import emu.skyline.SettingsActivity
import emu.skyline.di.getSettings

class KeyPickerPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

            context.getSettings().refreshRequired = true

            val success = KeyReader.import(context, uri, KeyReader.KeyType.parse(key))
            Snackbar.make((context as SettingsActivity).binding.root, if (success) R.string.import_keys_success else R.string.import_keys_failed, Snackbar.LENGTH_LONG).show()
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("*/*"))
}
