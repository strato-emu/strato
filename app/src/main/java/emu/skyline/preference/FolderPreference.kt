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
import androidx.preference.R

/**
 * This preference shows the decoded URI of it's preference and launches [FolderActivity]
 */
class FolderPreference : Preference {
    /**
     * The directory the preference is currently set to
     */
    private var mDirectory : String? = null

    constructor(context : Context?, attrs : AttributeSet?, defStyleAttr : Int) : super(context, attrs, defStyleAttr) {
        summaryProvider = SimpleSummaryProvider()
    }

    constructor(context : Context?, attrs : AttributeSet?) : this(context, attrs, R.attr.preferenceStyle)

    constructor(context : Context?) : this(context, null)

    /**
     * This launches [FolderActivity] on click to change the directory
     */
    override fun onClick() {
        val intent = Intent(context, FolderActivity::class.java)
        (context as Activity).startActivityForResult(intent, 0)
    }

    /**
     * This sets the initial value of [mDirectory]
     */
    override fun onSetInitialValue(defaultValue : Any?) {
        mDirectory = getPersistedString(defaultValue as String?)
    }

    /**
     * This [Preference.SummaryProvider] is used to set the summary for URI values
     */
    private class SimpleSummaryProvider : SummaryProvider<FolderPreference> {
        /**
         * This returns the decoded URI of the directory as the summary
         */
        override fun provideSummary(preference : FolderPreference) : CharSequence {
            return Uri.decode(preference.mDirectory) ?: ""
        }
    }
}
