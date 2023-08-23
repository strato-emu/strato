/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import com.google.android.material.snackbar.Snackbar
import org.stratoemu.strato.KeyReader
import org.stratoemu.strato.R
import org.stratoemu.strato.settings.SettingsActivity
import org.stratoemu.strato.di.getSettings

class KeyPickerPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

            context.getSettings().refreshRequired = true

            val result = KeyReader.import(context, uri, KeyReader.KeyType.parse(key))
            Snackbar.make((context as SettingsActivity).binding.root, resolveImportResultString(result), Snackbar.LENGTH_LONG).show()
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("*/*"))

    private fun resolveImportResultString(result : KeyReader.ImportResult) = when (result) {
        KeyReader.ImportResult.Success -> R.string.import_keys_success
        KeyReader.ImportResult.InvalidInputPath -> R.string.import_keys_invalid_input_path
        KeyReader.ImportResult.InvalidKeys -> R.string.import_keys_invalid_keys
        KeyReader.ImportResult.DeletePreviousFailed -> R.string.import_keys_delete_previous_failed
        KeyReader.ImportResult.MoveFailed -> R.string.import_keys_move_failed
    }
}
