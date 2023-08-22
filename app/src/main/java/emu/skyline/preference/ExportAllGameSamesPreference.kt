/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.provider.DocumentsContract
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.documentfile.provider.DocumentFile
import androidx.preference.Preference
import androidx.preference.R
import com.google.android.material.snackbar.Snackbar
import emu.skyline.getPublicFilesDir
import emu.skyline.provider.DocumentsProvider
import emu.skyline.settings.SettingsActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

class ExportAllGameSamesPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {

    private var lastZipCreated : File? = null
    private val savesFolderRoot by lazy { "${context.getPublicFilesDir().canonicalPath}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001/" }
    private val startForResultExportSave = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        lastZipCreated?.delete()
    }

    init {
        val saveExists = File(savesFolderRoot).exists()
        isEnabled = saveExists
        if (!saveExists){
            summary = context.getString(emu.skyline.R.string.export_all_game_saves_summary_no_save)
        }
    }

    private fun exportSave() {
        CoroutineScope(Dispatchers.IO).launch {
            val wasZipCreated = zipSave(savesFolderRoot)
            val lastZipFile = lastZipCreated
            if (!wasZipCreated || lastZipFile == null) {
                withContext(Dispatchers.Main) {
                    Snackbar.make((context as SettingsActivity).binding.root, emu.skyline.R.string.error, Snackbar.LENGTH_LONG).show()
                }
                return@launch
            }

            withContext(Dispatchers.Main) {
                val file = DocumentFile.fromSingleUri(context, DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001/${lastZipFile.name}"))!!
                val intent = Intent(Intent.ACTION_SEND)
                    .setDataAndType(file.uri, "application/zip")
                    .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                    .putExtra(Intent.EXTRA_STREAM, file.uri)
                startForResultExportSave.launch(Intent.createChooser(intent, "Share save file"))
            }
        }
    }

    override fun onClick() = exportSave()

    private fun zipSave(saveFolderPath : String) : Boolean {
        try {
            val saveFolder = File(saveFolderPath)
            val outputZipFileName = "${context.getString(emu.skyline.R.string.app_name)} saves - ${LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm"))}.zip"
            val outputZipFile = File("${savesFolderRoot}${outputZipFileName}")
            lastZipCreated?.delete()
            outputZipFile.createNewFile()
            ZipOutputStream(BufferedOutputStream(FileOutputStream(outputZipFile))).use { zos ->
                saveFolder.walkTopDown().forEach { file ->
                    val zipFileName = file.absolutePath.removePrefix(savesFolderRoot).removePrefix("/")
                    if (zipFileName == "" || zipFileName == outputZipFileName || zipFileName == savesFolderRoot.removePrefix("/").removeSuffix("/"))
                        return@forEach
                    val entry = ZipEntry("$zipFileName${(if (file.isDirectory) "/" else "")}")
                    zos.putNextEntry(entry)
                    if (file.isFile)
                        file.inputStream().use { fis -> fis.copyTo(zos) }
                }
            }
            lastZipCreated = outputZipFile
        } catch (e : Exception) {
            return false
        }
        return true
    }

}
