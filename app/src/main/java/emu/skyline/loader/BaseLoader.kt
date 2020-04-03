/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.loader

import android.content.ContentResolver
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.provider.OpenableColumns
import emu.skyline.utility.RandomAccessDocument
import java.io.IOException
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable
import java.util.*

/**
 * An enumeration of all supported ROM formats
 */
enum class RomFormat {
    NRO, XCI, NSP
}

/**
 * This resolves the format of a ROM from it's URI so we can determine formats for ROMs launched from arbitrary locations
 *
 * @param uri The URL of the ROM
 * @param contentResolver The instance of ContentResolver associated with the current context
 */
fun getRomFormat(uri: Uri, contentResolver: ContentResolver): RomFormat {
    var uriStr = ""
    contentResolver.query(uri, null, null, null, null)?.use { cursor ->
        val nameIndex: Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        cursor.moveToFirst()
        uriStr = cursor.getString(nameIndex)
    }
    return RomFormat.valueOf(uriStr.substring(uriStr.lastIndexOf(".") + 1).toUpperCase(Locale.ROOT))
}

/**
 * This class is used to hold an application's metadata in a serializable way
 */
class AppEntry : Serializable {
    /**
     * The name of the application
     */
    var name: String

    /**
     * The author of the application, if it can be extracted from the metadata
     */
    var author: String? = null

    var icon: Bitmap? = null

    /**
     * The format of the application ROM
     */
    var format: RomFormat

    /**
     * The URI of the application ROM
     */
    var uri: Uri

    constructor(name: String, author: String, format: RomFormat, uri: Uri, icon: Bitmap) {
        this.name = name
        this.author = author
        this.icon = icon
        this.format = format
        this.uri = uri
    }

    constructor(context: Context, format: RomFormat, uri: Uri) {
        this.name = context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex: Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            cursor.moveToFirst()
            cursor.getString(nameIndex)
        }!!
        this.format = format
        this.uri = uri
    }

    /**
     * This serializes this object into an OutputStream
     *
     * @param output The stream to which the object is written into
     */
    @Throws(IOException::class)
    private fun writeObject(output: ObjectOutputStream) {
        output.writeUTF(name)
        output.writeObject(format)
        output.writeUTF(uri.toString())
        output.writeBoolean(author != null)
        if (author != null)
            output.writeUTF(author)
        output.writeBoolean(icon != null)
        if (icon != null)
            icon!!.compress(Bitmap.CompressFormat.WEBP, 100, output)
    }

    /**
     * This initializes the object from an InputStream
     *
     * @param input The stream from which the object data is retrieved from
     */
    @Throws(IOException::class, ClassNotFoundException::class)
    private fun readObject(input: ObjectInputStream) {
        name = input.readUTF()
        format = input.readObject() as RomFormat
        uri = Uri.parse(input.readUTF())
        if (input.readBoolean())
            author = input.readUTF()
        if (input.readBoolean())
            icon = BitmapFactory.decodeStream(input)
    }
}

/**
 * This class is used as the base class for all loaders
 */
internal abstract class BaseLoader(val context: Context, val format: RomFormat) {
    /**
     * This returns an AppEntry object for the supplied document
     */
    abstract fun getAppEntry(file: RandomAccessDocument, uri: Uri): AppEntry

    /**
     * This returns if the supplied document is a valid ROM or not
     */
    abstract fun verifyFile(file: RandomAccessDocument): Boolean
}
