/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.utils

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.provider.DocumentsContract
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.documentfile.provider.DocumentFile
import androidx.fragment.app.FragmentActivity
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.getPublicFilesDir
import emu.skyline.provider.DocumentsProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.FilenameFilter
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

interface SaveManagementUtils {
    companion object {
        private val savesFolderRoot = "${SkylineApplication.instance.getPublicFilesDir().canonicalPath}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001"
        private var lastZipCreated : File? = null
        private lateinit var documentPicker : ActivityResultLauncher<Array<String>>
        private lateinit var startForResultExportSave : ActivityResultLauncher<Intent>
        var specificWorkUI = {}

        fun registerActivityResults(context : Context) {
            documentPicker = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
                it?.let { uri -> importSave(context, uri) }
            }
            startForResultExportSave = context.registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
                File(context.getPublicFilesDir().canonicalPath, "temp").deleteRecursively()
            }
        }

        fun registerActivityResults(fragmentAct : FragmentActivity) {
            val activity = fragmentAct as AppCompatActivity

            val activityResultRegistry = fragmentAct.activityResultRegistry
            startForResultExportSave = activityResultRegistry.register("startForResultExportSaveKey", ActivityResultContracts.StartActivityForResult()) {
                File(activity.getPublicFilesDir().canonicalPath, "temp").deleteRecursively()
            }
            documentPicker = activityResultRegistry.register("documentPickerKey", ActivityResultContracts.OpenDocument()) {
                it?.let { uri -> importSave(activity, uri) }
            }
        }

        /**
         * Checks if the saves folder exists.
         */
        fun savesFolderRootExists() : Boolean {
            return File(savesFolderRoot).exists()
        }

        /**
         * Checks if the save folder for the given game exists.
         */
        fun saveFolderGameExists(titleId : String?) : Boolean {
            if (titleId == null) return false
            return File(savesFolderRoot, titleId).exists()
        }

        /**
         * @return The folder containing the save file for the given game.
         */
        fun getSaveFolderGame(titleId : String) : File {
            return File(savesFolderRoot, titleId)
        }

        /**
         * Zips the save file located in the given folder path and creates a new zip file with the given name, and the current date and time.
         * @param saveFolderPath The path to the folder containing the save file to zip.
         * @param outputZipName The initial part of the name of the zip file to create.
         * @return true if the zip file is successfully created, false otherwise.
         */
        private fun zipSave(saveFolderPath : String, outputZipName : String) : Boolean {
            try {
                val tempFolder = File(SkylineApplication.instance.getPublicFilesDir().canonicalPath, "temp")
                tempFolder.mkdirs()

                val saveFolder = File(saveFolderPath)
                val outputZipFile = File(tempFolder, "$outputZipName - ${LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm"))}.zip")
                outputZipFile.createNewFile()
                ZipOutputStream(BufferedOutputStream(FileOutputStream(outputZipFile))).use { zos ->
                    saveFolder.walkTopDown().forEach { file ->
                        val zipFileName = file.absolutePath.removePrefix(savesFolderRoot).removePrefix("/")
                        if (zipFileName == "") return@forEach
                        val entry = ZipEntry("$zipFileName${(if (file.isDirectory) "/" else "")}")
                        zos.putNextEntry(entry)
                        if (file.isFile) file.inputStream().use { fis -> fis.copyTo(zos) }
                    }
                }
                lastZipCreated = outputZipFile
            } catch (e : Exception) {
                return false
            }
            return true
        }

        /**
         * Exports the save file located in the given folder path by creating a zip file and sharing it via intent.
         * @param titleId The title ID of the game to export the save file of. If empty, export all save files.
         * @param outputZipName The initial part of the name of the zip file to create.
         */
        fun exportSave(context : Context, titleId : String?, outputZipName : String) {
            if (titleId == null) return
            CoroutineScope(Dispatchers.IO).launch {
                val saveFolderPath = "$savesFolderRoot/$titleId"
                val wasZipCreated = zipSave(saveFolderPath, outputZipName)
                val lastZipFile = lastZipCreated
                if (!wasZipCreated || lastZipFile == null) {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
                    }
                    return@launch
                }

                withContext(Dispatchers.Main) {
                    val file = DocumentFile.fromSingleUri(context, DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/temp/${lastZipFile.name}"))!!
                    val intent = Intent(Intent.ACTION_SEND).setDataAndType(file.uri, "application/zip").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION).putExtra(Intent.EXTRA_STREAM, file.uri)
                    startForResultExportSave.launch(Intent.createChooser(intent, "Share save file"))
                }
            }
        }

        /**
         * Launches the document picker to import a save file.
         */
        fun importSave() {
            documentPicker.launch(arrayOf("application/zip"))
        }

        /**
         * Imports the save files contained in the zip file, and replaces any existing ones with the new save file.
         * @param zipUri The Uri of the zip file containing the save file(s) to import.
         */
        private fun importSave(context : Context, zipUri : Uri) {
            val inputZip = SkylineApplication.instance.contentResolver.openInputStream(zipUri)
            // A zip needs to have at least one subfolder named after a TitleId in order to be considered valid.
            var validZip = false
            val savesFolder = File(savesFolderRoot)
            val cacheSaveDir = File("${SkylineApplication.instance.cacheDir.path}/saves/")
            cacheSaveDir.mkdir()

            if (inputZip == null) {
                Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
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
                            Toast.makeText(context, R.string.save_file_invalid_zip_structure, Toast.LENGTH_LONG).show()
                            return@withContext
                        }
                        specificWorkUI()
                        Toast.makeText(context, R.string.save_file_imported_ok, Toast.LENGTH_LONG).show()
                    }

                    cacheSaveDir.deleteRecursively()
                }
            } catch (e : Exception) {
                Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
            }
        }

        /**
         * Deletes the save file for a given game.
         */
        fun deleteSaveFile(titleId : String?) : Boolean {
            if (titleId == null) return false
            File("$savesFolderRoot/$titleId").deleteRecursively()
            return true
        }
    }
}