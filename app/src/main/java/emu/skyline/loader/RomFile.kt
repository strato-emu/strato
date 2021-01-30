/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.loader

import android.content.ContentResolver
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable
import java.util.*

/**
 * An enumeration of all supported ROM formats
 */
enum class RomFormat(val format : Int) {
    NRO(0),
    NSO(1),
    NCA(2),
    XCI(3),
    NSP(4),
}

/**
 * This resolves the format of a ROM from it's URI so we can determine formats for ROMs launched from arbitrary locations
 *
 * @param uri The URL of the ROM
 * @param contentResolver The instance of ContentResolver associated with the current context
 */
fun getRomFormat(uri : Uri, contentResolver : ContentResolver) : RomFormat {
    var uriStr = ""
    contentResolver.query(uri, null, null, null, null)?.use { cursor ->
        val nameIndex : Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        cursor.moveToFirst()
        uriStr = cursor.getString(nameIndex)
    }
    return RomFormat.valueOf(uriStr.substring(uriStr.lastIndexOf(".") + 1).toUpperCase(Locale.ROOT))
}

/**
 * An enumeration of all possible results when populating [RomFile]
 */
enum class LoaderResult(val value : Int) {
    Success(0),
    ParsingError(1),
    MissingHeaderKey(2),
    MissingTitleKey(3),
    MissingTitleKek(4),
    MissingKeyArea(5);

    companion object {
        fun get(value : Int) = values().first { value == it.value }
    }
}

/**
 * This class is used to hold an application's metadata in a serializable way
 */
data class AppEntry(var name : String, var author : String?, var icon : Bitmap?, var format : RomFormat, var uri : Uri, var loaderResult : LoaderResult) : Serializable {
    constructor(context : Context, format : RomFormat, uri : Uri, loaderResult : LoaderResult) : this(context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
        val nameIndex : Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        cursor.moveToFirst()
        cursor.getString(nameIndex)
    }!!.dropLast(format.name.length + 1), null, null, format, uri, loaderResult)

    private fun writeObject(output : ObjectOutputStream) {
        output.writeUTF(name)
        output.writeObject(format)
        output.writeUTF(uri.toString())
        output.writeBoolean(author != null)
        if (author != null)
            output.writeUTF(author)
        output.writeInt(loaderResult.value)
        output.writeBoolean(icon != null)
        icon?.let {
            @Suppress("DEPRECATION")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                it.compress(Bitmap.CompressFormat.WEBP_LOSSY, 100, output)
            else
                it.compress(Bitmap.CompressFormat.WEBP, 100, output)
        }
    }

    private fun readObject(input : ObjectInputStream) {
        name = input.readUTF()
        format = input.readObject() as RomFormat
        uri = Uri.parse(input.readUTF())
        if (input.readBoolean())
            author = input.readUTF()
        loaderResult = LoaderResult.get(input.readInt())
        if (input.readBoolean())
            icon = BitmapFactory.decodeStream(input)
    }
}

/**
 * This class is used as interface between libskyline and Kotlin for loaders
 */
internal class RomFile(context : Context, format : RomFormat, uri : Uri) {
    /**
     * @note This field is filled in by native code
     */
    private var applicationName : String? = null

    /**
     * @note This field is filled in by native code
     */
    private var applicationAuthor : String? = null

    /**
     * @note This field is filled in by native code
     */
    private var rawIcon : ByteArray? = null

    val appEntry : AppEntry

    var result = LoaderResult.Success

    val valid : Boolean
        get() = result == LoaderResult.Success

    init {
        System.loadLibrary("skyline")

        context.contentResolver.openFileDescriptor(uri, "r")!!.use {
            result = LoaderResult.get(populate(format.ordinal, it.fd, context.filesDir.canonicalPath + "/"))
        }

        appEntry = applicationName?.let { name ->
            applicationAuthor?.let { author ->
                rawIcon?.let { icon ->
                    AppEntry(name, author, BitmapFactory.decodeByteArray(icon, 0, icon.size), format, uri, result)
                }
            }
        } ?: AppEntry(context, format, uri, result)
    }

    /**
     * Parses ROM and writes its metadata to [applicationName], [applicationAuthor] and [rawIcon]
     * @param format The format of the ROM
     * @param romFd A file descriptor of the ROM
     * @param appFilesPath Path to internal app data storage, needed to read imported keys
     * @return A pointer to the newly allocated object, or 0 if the ROM is invalid
     */
    private external fun populate(format : Int, romFd : Int, appFilesPath : String) : Int
}