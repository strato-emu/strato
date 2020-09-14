/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.AttributeSet
import androidx.preference.Preference
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import emu.skyline.KeyReader
import emu.skyline.R
import emu.skyline.SettingsActivity
import kotlinx.android.synthetic.main.settings_activity.*

/**
 * Launches [FileActivity] and process the selected file for key import
 */
class FilePreference @JvmOverloads constructor(context : Context?, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr), ActivityResultDelegate {
    override var requestCode = 0

    override fun onClick() = (context as Activity).startActivityForResult(Intent(context, FileActivity::class.java).apply { putExtra(DocumentActivity.KEY_NAME, key) }, requestCode)

    override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        if (this.requestCode == requestCode) {
            if (key == "prod_keys" || key == "title_keys") {
                val success = KeyReader.import(
                        context,
                        Uri.parse(PreferenceManager.getDefaultSharedPreferences(context).getString(key, "")),
                        KeyReader.KeyType.parse(key)
                )
                Snackbar.make((context as SettingsActivity).settings, if (success) R.string.import_keys_success else R.string.import_keys_failed, Snackbar.LENGTH_LONG).show()
            }
        }
    }
}
