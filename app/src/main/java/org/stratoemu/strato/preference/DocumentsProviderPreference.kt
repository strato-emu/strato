/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.preference

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import android.util.AttributeSet
import androidx.preference.Preference
import androidx.preference.R as AndroidR
import com.google.android.material.snackbar.Snackbar
import org.stratoemu.strato.R
import org.stratoemu.strato.settings.SettingsActivity
import org.stratoemu.strato.provider.DocumentsProvider

class DocumentsProviderPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = AndroidR.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private fun launchOpenIntent(action : String) : Boolean {
        return try {
            val intent = Intent(action)
            intent.addCategory(Intent.CATEGORY_DEFAULT)
            intent.data = DocumentsContract.buildRootUri(DocumentsProvider.AUTHORITY, DocumentsProvider.ROOT_ID)
            intent.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or Intent.FLAG_GRANT_PREFIX_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            context.startActivity(intent)
            true
        } catch (e: ActivityNotFoundException) {
            false
        }
    }

    override fun onClick() {
        if (launchOpenIntent(Intent.ACTION_VIEW) or launchOpenIntent("android.provider.action.BROWSE"))
            return
        Snackbar.make((context as SettingsActivity).binding.root, R.string.open_data_directory_failed, Snackbar.LENGTH_SHORT).show()
    }
}
