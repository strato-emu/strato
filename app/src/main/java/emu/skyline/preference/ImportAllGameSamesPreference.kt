/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.net.Uri
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.R
import com.google.android.material.snackbar.Snackbar
import emu.skyline.getPublicFilesDir
import emu.skyline.settings.SettingsActivity
import emu.skyline.utils.ZipUtils
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FilenameFilter

/**
 * Imports all save files contained in the zip file, and replaces any existing ones with the new save file.
 */
class ImportAllGameSamesPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {


    private val savesFolderRoot by lazy { "${context.getPublicFilesDir().canonicalPath}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001/" }

    private val documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri -> importSave(uri) }
    }
    private val binding = (context as SettingsActivity).binding
    private fun requireContext() : ComponentActivity {
        return (context as ComponentActivity)
    }

    private fun importSave(zipUri : Uri) {
        val inputZip = requireContext().contentResolver.openInputStream(zipUri)
        // A zip needs to have at least one subfolder named after a TitleId in order to be considered valid.
        var validZip = false
        val savesFolder = File(savesFolderRoot)
        val cacheSaveDir = File("${requireContext().cacheDir.path}/saves/")
        cacheSaveDir.mkdir()

        if (inputZip == null) {
            Snackbar.make(binding.root, emu.skyline.R.string.error, Snackbar.LENGTH_LONG).show()
            return
        }
        val filterTitleId = FilenameFilter { _, dirName -> dirName.matches(Regex("^0100[\\dA-Fa-f]{12}$")) }

        try {
            CoroutineScope(Dispatchers.IO).launch {
                ZipUtils.unzip(inputZip, cacheSaveDir)

                cacheSaveDir.list(filterTitleId)?.forEach { savePath ->
                    File(savesFolder, savePath).deleteRecursively()
                    File(cacheSaveDir, savePath).copyRecursively(File(savesFolder, savePath), true)
                    validZip = true
                }

                withContext(Dispatchers.Main) {
                    if (!validZip) {
                        Snackbar.make(binding.root, emu.skyline.R.string.save_file_invalid_zip_structure, Snackbar.LENGTH_LONG).show()
                        return@withContext
                    }
                    Snackbar.make(binding.root, emu.skyline.R.string.save_file_imported_ok, Snackbar.LENGTH_LONG).show()
                }

                cacheSaveDir.deleteRecursively()
            }
        } catch (e : Exception) {
            Snackbar.make(binding.root, emu.skyline.R.string.error, Snackbar.LENGTH_LONG).show()
        }
    }

    override fun onClick() = documentPicker.launch(arrayOf("application/zip"))
}
