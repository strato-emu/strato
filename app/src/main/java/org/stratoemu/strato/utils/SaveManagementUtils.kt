/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package org.stratoemu.strato.utils

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
import org.stratoemu.strato.R
import org.stratoemu.strato.StratoApplication
import org.stratoemu.strato.getPublicFilesDir
import org.stratoemu.strato.provider.DocumentsProvider
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.io.FilenameFilter
import java.io.IOException
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

interface SaveManagementUtils {

    companion object {
        private val savesFolderRoot = "${StratoApplication.instance.getPublicFilesDir().canonicalPath}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001"

        fun registerDocumentPicker(context : Context) : ActivityResultLauncher<Array<String>> {
            return (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.OpenDocument()) {
                it?.let { uri -> importSave(context, uri) }
            }
        }

        fun registerDocumentPicker(fragmentAct : FragmentActivity, onImportComplete : () -> Unit = {}) : ActivityResultLauncher<Array<String>> {
            val activity = fragmentAct as AppCompatActivity
            val activityResultRegistry = fragmentAct.activityResultRegistry

            return activityResultRegistry.register("documentPickerKey", ActivityResultContracts.OpenDocument()) {
                it?.let { uri -> importSave(activity, uri, onImportComplete) }
            }
        }

        fun registerStartForResultExportSave(context : Context) : ActivityResultLauncher<Intent> {
            return (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
                File(context.getPublicFilesDir().canonicalPath, "temp").deleteRecursively()
            }
        }

        fun registerStartForResultExportSave(fragmentAct : FragmentActivity) : ActivityResultLauncher<Intent> {
            val activity = fragmentAct as AppCompatActivity
            val activityResultRegistry = fragmentAct.activityResultRegistry

            return activityResultRegistry.register("startForResultExportSaveKey", ActivityResultContracts.StartActivityForResult()) {
                File(activity.getPublicFilesDir().canonicalPath, "temp").deleteRecursively()
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
         * @return the zip file created.
         */
        private fun zipSave(saveFolderPath : String, outputZipName : String) : File? {
            try {
                val tempFolder = File(StratoApplication.instance.getPublicFilesDir().canonicalPath, "temp")
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
                return outputZipFile
            } catch (e : Exception) {
                return null
            }
        }

        /**
         * Exports the save file located in the given folder path by creating a zip file and sharing it via intent.
         * @param titleId The title ID of the game to export the save file of. If empty, export all save files.
         * @param outputZipName The initial part of the name of the zip file to create.
         */
        fun exportSave(context : Context, startForResultExportSave : ActivityResultLauncher<Intent>, titleId : String?, outputZipName : String) {
            if (titleId == null) return
            CoroutineScope(Dispatchers.IO).launch {
                val saveFolderPath = "$savesFolderRoot/$titleId"
                val zipCreated = zipSave(saveFolderPath, outputZipName)
                if (zipCreated == null) {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
                    }
                    return@launch
                }

                withContext(Dispatchers.Main) {
                    val file = DocumentFile.fromSingleUri(context, DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/temp/${zipCreated.name}"))!!
                    val intent = Intent(Intent.ACTION_SEND).setDataAndType(file.uri, "application/zip").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION).putExtra(Intent.EXTRA_STREAM, file.uri)
                    startForResultExportSave.launch(Intent.createChooser(intent, context.getString(R.string.save_file_share)))
                }
            }
        }

        /**
         * Launches the document picker to import a save file.
         */
        fun importSave(documentPicker : ActivityResultLauncher<Array<String>>) {
            documentPicker.launch(arrayOf("application/zip"))
        }

        /**
         * Imports the save files contained in the zip file, and replaces any existing ones with the new save file.
         * @param zipUri The Uri of the zip file containing the save file(s) to import.
         */
        private fun importSave(context : Context, zipUri : Uri, onImportComplete : () -> Unit = {}) {
            val inputZip = StratoApplication.instance.contentResolver.openInputStream(zipUri)
            // A zip needs to have at least one subfolder named after a TitleId in order to be considered valid.
            var validZip = false
            val savesFolder = File(savesFolderRoot)
            val cacheSaveDir = File("${StratoApplication.instance.cacheDir.path}/saves/")
            cacheSaveDir.mkdir()

            if (inputZip == null) {
                Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
                return
            }

            val filterTitleId = FilenameFilter { _, dirName -> dirName.matches(Regex("^0100[\\dA-Fa-f]{12}$")) }

            CoroutineScope(Dispatchers.IO).launch {
                try {
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
                        onImportComplete()
                        Toast.makeText(context, R.string.save_file_imported_ok, Toast.LENGTH_LONG).show()
                    }
                } catch (e : IOException) {
                    withContext(Dispatchers.Main) {
                        Toast.makeText(context, R.string.error, Toast.LENGTH_LONG).show()
                    }
                } finally {
                    cacheSaveDir.deleteRecursively()
                }
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