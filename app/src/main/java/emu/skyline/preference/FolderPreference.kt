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
import androidx.preference.Preference.SummaryProvider
import androidx.preference.R

/**
 * This preference shows the decoded URI of it's preference and launches [DocumentActivity]
 */
class FolderPreference @JvmOverloads constructor(context : Context?, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : ActivityResultPreference(context, attrs, defStyleAttr) {
    init {
        summaryProvider = SummaryProvider<FolderPreference> { preference ->
            Uri.decode(preference.getPersistedString(""))
        }
    }

    /**
     * This launches [DocumentActivity] on click to change the directory
     */
    override fun onClick() {
        (context as Activity).startActivityForResult(Intent(context, FolderActivity::class.java).apply { putExtra(DocumentActivity.KEY_NAME, key) }, requestCode)
    }

    override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        if (requestCode == requestCode) notifyChanged()
    }
}
