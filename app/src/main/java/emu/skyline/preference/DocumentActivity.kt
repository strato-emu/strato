/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.utils.Settings
import javax.inject.Inject

/**
 * This activity is used to launch a document picker and saves the result to preferences
 */
@AndroidEntryPoint
abstract class DocumentActivity : AppCompatActivity() {
    companion object {
        const val KEY_NAME = "key_name"
    }

    private lateinit var keyName : String

    protected abstract val actionIntent : Intent

    @Inject
    lateinit var settings : Settings

    /**
     * This launches the [Intent.ACTION_OPEN_DOCUMENT_TREE] intent on creation
     */
    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        keyName = intent.getStringExtra(KEY_NAME)!!

        this.startActivityForResult(actionIntent, 1)
    }

    /**
     * This changes the search location preference if the [Intent.ACTION_OPEN_DOCUMENT_TREE] has returned and [finish]es the activity
     */
    public override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (resultCode == Activity.RESULT_OK && requestCode == 1) {
            val uri = data!!.data!!

            contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

            settings.refreshRequired = true
            PreferenceManager.getDefaultSharedPreferences(this).edit()
                    .putString(keyName, uri.toString())
                    .apply()
        }
        setResult(resultCode)
        finish()
    }
}
