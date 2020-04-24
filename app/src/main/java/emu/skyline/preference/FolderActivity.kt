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

/**
 * This activity is used to select a new search location and set preferences to reflect that
 */
class FolderActivity : AppCompatActivity() {
    /**
     * This launches the [Intent.ACTION_OPEN_DOCUMENT_TREE] intent on creation
     */
    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE)
        this.startActivityForResult(intent, 1)
    }

    /**
     * This changes the search location preference if the [Intent.ACTION_OPEN_DOCUMENT_TREE] has returned and [finish]es the activity
     */
    public override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        if (resultCode == Activity.RESULT_OK) {
            if (requestCode == 1) {
                val uri = data!!.data!!

                contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)

                PreferenceManager.getDefaultSharedPreferences(this).edit()
                        .putString("search_location", uri.toString())
                        .putBoolean("refresh_required", true)
                        .apply()
            }
        }

        finish()
    }
}
