/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.activity.result.ActivityResultLauncher
import androidx.preference.Preference
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.stratoemu.strato.R
import org.stratoemu.strato.utils.SaveManagementUtils

class ImportExportSavesPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val documentPicker = SaveManagementUtils.registerDocumentPicker(context)
    private val startForResultExportSave = SaveManagementUtils.registerStartForResultExportSave(context)

    override fun onClick() {
        val saveDataExists = SaveManagementUtils.savesFolderRootExists()
        val dialog = MaterialAlertDialogBuilder(context)
            .setTitle(R.string.save_management)
            .setPositiveButton(R.string.import_save) { _, _ ->
                SaveManagementUtils.importSave(documentPicker)
            }.setNeutralButton(android.R.string.cancel, null)

        if (saveDataExists) {
            dialog.setMessage(R.string.save_data_found)
                .setNegativeButton(R.string.export_save) { _, _ ->
                    SaveManagementUtils.exportSave(context, startForResultExportSave, "", context.getString(R.string.global_save_data_zip_name))
                }
        } else {
            dialog.setMessage(R.string.save_data_not_found)
        }

        dialog.show()
    }
}
