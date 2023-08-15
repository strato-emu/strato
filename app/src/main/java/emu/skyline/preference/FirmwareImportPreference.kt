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
    private class Firmware(val valid : Boolean, val version : String)

    private val firmwarePath = File(context.getPublicFilesDir().canonicalPath + "/switch/nand/system/Contents/registered/")
    private val keysPath = "${context.filesDir.canonicalPath}/keys/"
    private val fontsPath = "${context.getPublicFilesDir().canonicalPath}/fonts/"

    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            val inputZip = context.contentResolver.openInputStream(uri)
            if (inputZip == null) {
                Snackbar.make((context as SettingsActivity).binding.root, R.string.error, Snackbar.LENGTH_LONG).show()
                return@registerForActivityResult
            }

            val cacheFirmwareDir = File("${context.cacheDir.path}/registered/")

            val task : () -> Any = {
                var messageToShow : Int

                try {
                    // Unzip in cache dir to not delete previous firmware in case the zip given doesn't contain a valid one
                    ZipUtils.unzip(inputZip, cacheFirmwareDir)

                    val firmware = isFirmwareValid(cacheFirmwareDir)
                    messageToShow = if (!firmware.valid) {
                        R.string.import_firmware_invalid_contents
                    } else {
                        firmwarePath.deleteRecursively()
                        cacheFirmwareDir.copyRecursively(firmwarePath, true)
                        PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, firmware.version).apply()
                        extractFonts(firmwarePath.path, keysPath, fontsPath)
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
        val keysDir = File(keysPath)
        isEnabled = keysDir.exists() && keysDir.listFiles()?.isNotEmpty() == true

        summaryProvider = SummaryProvider<FirmwareImportPreference> { preference ->
            getFirmwareStringRes(preference)
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("application/zip"))

    /**
     * Checks if the given directory stores a valid firmware. For that, all files must be NCAs and
     * one of them must store the firmware version.
     * @return A pair that tells if the firmware is valid, and if so, which firmware version it is
     */
    private fun isFirmwareValid(cacheFirmwareDir : File) : Firmware {
        val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }

        val unfilteredNumOfFiles = cacheFirmwareDir.list()?.size ?: -1
        val filteredNumOfFiles = cacheFirmwareDir.list(filterNCA)?.size ?: -2

        return if (unfilteredNumOfFiles == filteredNumOfFiles) {
            val version = fetchFirmwareVersion(cacheFirmwareDir.path, keysPath)
            Firmware(version.isNotEmpty(), version)
        } else Firmware(false, "")
    }

    private fun getFirmwareStringRes(preference : Preference) : String {
        if (!preference.isEnabled) {
            return context.getString(R.string.firmware_keys_needed)
        }

        val noFirmwareInstalled = context.getString(R.string.firmware_not_installed)
        val storedString = getPersistedString((noFirmwareInstalled))
        val firmwarePathEmpty = !firmwarePath.exists() || firmwarePath.listFiles()?.isEmpty() == true

        return if (storedString == noFirmwareInstalled && !firmwarePathEmpty)
            fetchFirmwareVersion(firmwarePath.path, keysPath)
        else if (storedString != noFirmwareInstalled && firmwarePathEmpty)
            noFirmwareInstalled
        else
            storedString
    }

    private external fun fetchFirmwareVersion(systemArchivesPath : String, keysPath : String) : String
    private external fun extractFonts(systemArchivesPath : String, keysPath : String, fontsPath: String)
}
