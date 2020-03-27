/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.content.Context
import android.os.ParcelFileDescriptor
import androidx.documentfile.provider.DocumentFile
import java.nio.ByteBuffer

class RandomAccessDocument(private var parcelFileDescriptor: ParcelFileDescriptor) {
    constructor(context: Context, file: DocumentFile) : this(context.contentResolver.openFileDescriptor(file.uri, "r")!!)

    private val fileDescriptor = parcelFileDescriptor.fileDescriptor
    private var position: Long = 0

    fun read(array: ByteArray): Int {
        val bytesRead = android.system.Os.pread(fileDescriptor, array, 0, array.size, position)
        position += bytesRead
        return bytesRead
    }

    fun read(buffer: ByteBuffer): Int {
        val bytesRead = android.system.Os.pread(fileDescriptor, buffer.array(), 0, buffer.array().size, position)
        position += bytesRead
        return bytesRead
    }

    fun readLong(): Long {
        val buffer: ByteBuffer = ByteBuffer.allocate(Long.SIZE_BYTES)
        read(buffer)
        return buffer.long
    }

    fun readInt(): Int {
        val buffer: ByteBuffer = ByteBuffer.allocate(Int.SIZE_BYTES)
        read(buffer)
        return buffer.int
    }

    fun seek(position: Long) {
        this.position = position
    }

    fun skipBytes(position: Long) {
        this.position += position
    }

    fun close() {
        parcelFileDescriptor.close()
    }
}
