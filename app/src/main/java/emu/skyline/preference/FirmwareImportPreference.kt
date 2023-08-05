/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import com.google.android.material.snackbar.Snackbar
import emu.skyline.R
import emu.skyline.fragments.IndeterminateProgressDialogFragment
import emu.skyline.getPublicFilesDir
import emu.skyline.settings.SettingsActivity
import emu.skyline.utils.ZipUtils
import java.io.File
import java.io.FilenameFilter

class FirmwareImportPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            val inputZip = context.contentResolver.openInputStream(uri)
            if (inputZip == null) {
                Snackbar.make((context as SettingsActivity).binding.root, R.string.error, Snackbar.LENGTH_LONG).show()
                return@registerForActivityResult
            }

            val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }

            val firmwarePath = File(context.getPublicFilesDir().canonicalPath + "/switch/nand/system/Contents/registered/")
            val cacheFirmwareDir = File("${context.cacheDir.path}/registered/")

            val task : () -> Any = {
                var messageToShow : Any
                try {
                    ZipUtils.unzip(inputZip, cacheFirmwareDir)
                    val unfilteredNumOfFiles = cacheFirmwareDir.list()?.size ?: -1
                    val filteredNumOfFiles = cacheFirmwareDir.list(filterNCA)?.size ?: -2
                    messageToShow = if (unfilteredNumOfFiles != filteredNumOfFiles) {
                        Snackbar.make((context as SettingsActivity).binding.root, R.string.import_firmware_invalid_contents, Snackbar.LENGTH_LONG)
                    } else {
                        firmwarePath.deleteRecursively()
                        cacheFirmwareDir.copyRecursively(firmwarePath, true)
                        Snackbar.make((context as SettingsActivity).binding.root, R.string.import_firmware_success, Snackbar.LENGTH_LONG)
                    }
                } catch (e : Exception) {
                    messageToShow = Snackbar.make((context as SettingsActivity).binding.root, R.string.error, Snackbar.LENGTH_LONG)
                } finally {
                    cacheFirmwareDir.deleteRecursively()
                }
                messageToShow
            }

            IndeterminateProgressDialogFragment.newInstance(context as SettingsActivity, R.string.import_firmware_in_progress, task)
                .show(context.supportFragmentManager, IndeterminateProgressDialogFragment.TAG)
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("application/zip"))
}
