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
import androidx.preference.Preference.SummaryProvider
import androidx.preference.PreferenceManager
import com.google.android.material.snackbar.Snackbar
import emu.skyline.R
import emu.skyline.fragments.IndeterminateProgressDialogFragment
import emu.skyline.getPublicFilesDir
import emu.skyline.settings.SettingsActivity
import emu.skyline.utils.ZipUtils
import java.io.File
import java.io.FilenameFilter

class FirmwareImportPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    class Firmware(val valid : Boolean, val version : String)

    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            val inputZip = context.contentResolver.openInputStream(uri)
            if (inputZip == null) {
                Snackbar.make((context as SettingsActivity).binding.root, R.string.error, Snackbar.LENGTH_LONG).show()
                return@registerForActivityResult
            }

            val firmwarePath = File(context.getPublicFilesDir().canonicalPath + "/switch/nand/system/Contents/registered/")
            val cacheFirmwareDir = File("${context.cacheDir.path}/registered/")

            val task : () -> Any = {
                var messageToShow : Int

                try {
                    ZipUtils.unzip(inputZip, cacheFirmwareDir)

                    val firmware = isFirmwareValid(cacheFirmwareDir)
                    messageToShow = if (!firmware.valid) {
                        R.string.import_firmware_invalid_contents
                    } else {
                        firmwarePath.deleteRecursively()
                        cacheFirmwareDir.copyRecursively(firmwarePath, true)
                        PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, firmware.version).apply()
                        notifyChanged()
                        R.string.import_firmware_success
                    }
                } catch (e : Exception) {
                    messageToShow = R.string.error
                } finally {
                    cacheFirmwareDir.deleteRecursively()
                }

                Snackbar.make((context as SettingsActivity).binding.root, messageToShow, Snackbar.LENGTH_LONG)
            }

            IndeterminateProgressDialogFragment.newInstance(context as SettingsActivity, R.string.import_firmware_in_progress, task)
                .show(context.supportFragmentManager, IndeterminateProgressDialogFragment.TAG)
        }
    }

    init {
        summaryProvider = SummaryProvider<FirmwareImportPreference> { preference ->
            preference.getPersistedString("No firmware installed")
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("application/zip"))

    /**
     * Checks if the given directory stores a valid firmware
     * @return A pair that tells if the firmware is valid, and if so, which firmware version it is
     */
    private fun isFirmwareValid(cacheFirmwareDir : File) : Firmware {
        val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }

        val unfilteredNumOfFiles = cacheFirmwareDir.list()?.size ?: -1
        val filteredNumOfFiles = cacheFirmwareDir.list(filterNCA)?.size ?: -2

        return if (unfilteredNumOfFiles == filteredNumOfFiles) {
            val version = fetchFirmwareVersion(cacheFirmwareDir.path, context.filesDir.path + "/keys/")
            Firmware(version.isNotEmpty(), version)
        } else Firmware(false, "")
    }

    private external fun fetchFirmwareVersion(systemArchivesPath : String, keysPath : String) : String
}
