/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Context
import android.net.Uri
import android.util.Log
import androidx.documentfile.provider.DocumentFile
import java.io.File

object KeyReader {
    private val Tag = KeyReader::class.java.simpleName

    enum class ImportResult {
        Success,
        InvalidInputPath,
        InvalidKeys,
        DeletePreviousFailed,
        MoveFailed,
    }

    enum class KeyType(val keyName : String, val fileName : String) {
        Title("title_keys", "title.keys"), Prod("prod_keys", "prod.keys");

        companion object {
            fun parse(keyName : String) = values().first { it.keyName == keyName }
            fun parse(documentFile : DocumentFile) = values().first { it.fileName == documentFile.name }
            fun parseOrNull(documentFile : DocumentFile) = values().find { it.fileName == documentFile.name }
        }
    }

    fun importFromLocation(context : Context, searchLocation : Uri) = importFromDirectory(context, DocumentFile.fromTreeUri(context, searchLocation)!!)

    private fun importFromDirectory(context : Context, directory : DocumentFile) {
        directory.listFiles().forEach { file ->
            if (file.isDirectory) {
                importFromDirectory(context, file)
            } else {
                KeyType.parseOrNull(file)?.let { import(context, file.uri, it) }
            }
        }
    }

    /**
     * Reads keys file, trims and writes to internal app data storage, it makes sure file is properly formatted
     */
    fun import(context : Context, uri : Uri, keyType : KeyType) : ImportResult {
        Log.i(Tag, "Parsing ${keyType.name} $uri")

        if (!DocumentFile.isDocumentUri(context, uri))
            return ImportResult.InvalidInputPath

        val outputDirectory = File("${context.filesDir.canonicalFile}/keys/")
        if (!outputDirectory.exists())
            outputDirectory.mkdirs()

        val outputFile = File(outputDirectory, keyType.fileName)
        val tmpOutputFile = File("${outputFile}.tmp")
        var valid = false

        context.contentResolver.openInputStream(uri).use { inputStream ->
            tmpOutputFile.bufferedWriter().use { writer ->
                valid = inputStream!!.bufferedReader().useLines {
                    for (line in it) {
                        if (line.startsWith(";") || line.isBlank()) continue

                        val pair = line.split("=")
                        if (pair.size != 2)
                            return@useLines false

                        val key = pair[0].trim()
                        val value = pair[1].trim()
                        when (keyType) {
                            KeyType.Title -> {
                                if (key.length != 32 && !isHexString(key))
                                    return@useLines false
                                if (value.length != 32 && !isHexString(value))
                                    return@useLines false
                            }
                            KeyType.Prod -> {
                                if (!key.contains("_"))
                                    return@useLines false
                                if (!isHexString(value))
                                    return@useLines false
                            }
                        }

                        writer.append("$key=$value\n")
                    }
                    true
                }
            }
        }

        val cleanup = {
            try {
                tmpOutputFile.delete()
            } catch (_ : Exception) {
            }
        }

        if (!valid) {
            cleanup()
            return ImportResult.InvalidKeys
        }

        if (outputFile.exists() && !outputFile.delete()) {
            cleanup()
            return ImportResult.DeletePreviousFailed
        }

        if (!tmpOutputFile.renameTo(outputFile)) {
            cleanup()
            return ImportResult.MoveFailed
        }

        return ImportResult.Success
    }

    private fun isHexString(str : String) : Boolean {
        for (c in str)
            if (!(c in '0'..'9' || c in 'a'..'f' || c in 'A'..'F')) return false
        return true
    }
}
