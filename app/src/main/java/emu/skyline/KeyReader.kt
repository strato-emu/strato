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

    enum class KeyType(val keyName : String, val fileName : String) {
        Title("title_keys", "title.keys"), Prod("prod_keys", "prod.keys");

        companion object {
            fun parse(keyName : String) = values().first { it.keyName == keyName }
        }
    }

    /**
     * Reads keys file, trims and writes to internal app data storage, it makes sure file is properly formatted
     */
    fun import(context : Context, uri : Uri, keyType : KeyType) : Boolean {
        Log.i(Tag, "Parsing ${keyType.name} $uri")

        if (!DocumentFile.isDocumentUri(context, uri))
            return false

        val tmpOutputFile = File("${context.filesDir.canonicalFile}/${keyType.fileName}.tmp")

        val inputStream = context.contentResolver.openInputStream(uri)
        tmpOutputFile.bufferedWriter().use { writer ->
            val valid = inputStream!!.bufferedReader().useLines {
                for (line in it) {
                    if (line.startsWith(";")) continue

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

            if (valid) tmpOutputFile.renameTo(File("${tmpOutputFile.parent}/${keyType.fileName}"))
            return valid
        }
    }

    private fun isHexString(str : String) : Boolean {
        for (c in str)
            if (!(c in '0'..'9' || c in 'a'..'f' || c in 'A'..'F')) return false
        return true
    }
}
