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
import android.os.ParcelFileDescriptor
import android.provider.OpenableColumns
import android.view.Surface
import java.io.IOException
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable
import java.util.*

/**
 * An enumeration of all supported ROM formats
 */
enum class RomFormat(val format: Int){
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
 * This class is used to hold an application's metadata in a serializable way
 */
class AppEntry : Serializable {
    /**
     * The name of the application
     */
    var name : String

    /**
     * The author of the application, if it can be extracted from the metadata
     */
    var author : String? = null

    var icon : Bitmap? = null

    /**
     * The format of the application ROM
     */
    var format : RomFormat

    /**
     * The URI of the application ROM
     */
    var uri : Uri

    constructor(name : String, author : String, format : RomFormat, uri : Uri, icon : Bitmap?) {
        this.name = name
        this.author = author
        this.icon = icon
        this.format = format
        this.uri = uri
    }

    constructor(context : Context, format : RomFormat, uri : Uri) {
        this.name = context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex : Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            cursor.moveToFirst()
            cursor.getString(nameIndex)
        }!!.dropLast(format.name.length + 1)
        this.format = format
        this.uri = uri
    }

    /**
     * This serializes this object into an OutputStream
     *
     * @param output The stream to which the object is written into
     */
    @Throws(IOException::class)
    private fun writeObject(output : ObjectOutputStream) {
        output.writeUTF(name)
        output.writeObject(format)
        output.writeUTF(uri.toString())
        output.writeBoolean(author != null)
        if (author != null)
            output.writeUTF(author)
        output.writeBoolean(icon != null)
        if (icon != null)
            icon!!.compress(Bitmap.CompressFormat.WEBP_LOSSY, 100, output)
    }

    /**
     * This initializes the object from an InputStream
     *
     * @param input The stream from which the object data is retrieved from
     */
    @Throws(IOException::class, ClassNotFoundException::class)
    private fun readObject(input : ObjectInputStream) {
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
 * This class is used as interface between libskyline and Kotlin for loaders
 */
internal class RomFile(val context : Context, val format : RomFormat, val file : ParcelFileDescriptor) : AutoCloseable {
    /**
     * This is a pointer to the corresponding C++ Loader class
     */
    var instance : Long

    init {
        System.loadLibrary("skyline")

        instance = initialize(format.ordinal, file.fd)
    }

    /**
     * This allocates and initializes a new loader object
     * @param format The format of the ROM
     * @param romFd A file descriptor of the ROM
     * @return A pointer to the newly allocated object, or 0 if the ROM is invalid
     */
    private external fun initialize(format : Int, romFd : Int) : Long

    /**
     * @return Whether the ROM contains assets, such as an icon or author information
     */
    private external fun hasAssets(instance : Long) : Boolean

    /**
     * @return A ByteArray containing the application's icon as a bitmap
     */
    private external fun getIcon(instance : Long) : ByteArray

    /**
     * @return A String containing the name of the application
     */
    private external fun getApplicationName(instance : Long) : String

    /**
     * @return A String containing the publisher of the application
     */
    private external fun getApplicationPublisher(instance : Long) : String

    /**
     * This destroys an existing loader object and frees it's resources
     */
    private external fun destroy(instance : Long)

    /**
     * This is used to get the [AppEntry] for the specified ROM
     */
    fun getAppEntry(uri : Uri) : AppEntry {
        return if (hasAssets(instance)) {
            val rawIcon = getIcon(instance)
            val icon = if (rawIcon.size != 0) BitmapFactory.decodeByteArray(rawIcon, 0, rawIcon.size) else null

            AppEntry(getApplicationName(instance), getApplicationPublisher(instance), format, uri, icon)
        } else {
            AppEntry(context, format, uri)
        }
    }

    /**
     * This checks if the currently loaded ROM is valid
     */
    fun valid() : Boolean {
        return instance != 0L
    }

    /**
     * This destroys the C++ loader object
     */
    override fun close() {
        if (valid()) {
            destroy(instance)
            instance = 0
        }
    }
}