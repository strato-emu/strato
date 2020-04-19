/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.content.Context
import android.os.ParcelFileDescriptor
import androidx.documentfile.provider.DocumentFile
import java.nio.ByteBuffer

/**
 * This is made as a parallel to [java.io.RandomAccessFile] for [DocumentFile]s
 *
 * @param parcelFileDescriptor The file descriptor for the [DocumentFile]
 */
class RandomAccessDocument(private var parcelFileDescriptor: ParcelFileDescriptor) {
    /**
     * The actual file descriptor for the [DocumentFile] as an [FileDescriptor] object
     */
    private val fileDescriptor = parcelFileDescriptor.fileDescriptor

    /**
     * The current position of where the file is being read
     */
    private var position: Long = 0

    /**
     * The constructor sets [parcelFileDescriptor] by opening a read-only FD to [file]
     */
    constructor(context: Context, file: DocumentFile) : this(context.contentResolver.openFileDescriptor(file.uri, "r")!!)

    /**
     * This reads in as many as possible bytes into [array] (Generally [array].size)
     *
     * @return The amount of bytes read from the file
     */
    fun read(array: ByteArray): Int {
        val bytesRead = android.system.Os.pread(fileDescriptor, array, 0, array.size, position)
        position += bytesRead
        return bytesRead
    }

    /**
     * This reads in as many as possible bytes into [buffer] (Generally [buffer].array().size)
     *
     * @return The amount of bytes read from the file
     */
    fun read(buffer: ByteBuffer): Int {
        val bytesRead = android.system.Os.pread(fileDescriptor, buffer.array(), 0, buffer.array().size, position)
        position += bytesRead
        return bytesRead
    }

    /**
     * This returns a single [Long] from the file at the current [position]
     */
    fun readLong(): Long {
        val buffer: ByteBuffer = ByteBuffer.allocate(Long.SIZE_BYTES)
        read(buffer)
        return buffer.long
    }

    /**
     * This returns a single [Int] from the file at the current [position]
     */
    fun readInt(): Int {
        val buffer: ByteBuffer = ByteBuffer.allocate(Int.SIZE_BYTES)
        read(buffer)
        return buffer.int
    }

    /**
     * This sets [RandomAccessDocument.position] to the supplied [position]
     */
    fun seek(position: Long) {
        this.position = position
    }

    /**
     * This increments [position] by [amount]
     */
    fun skipBytes(amount: Long) {
        this.position += amount
    }

    /**
     * This closes [parcelFileDescriptor] so this class doesn't leak file descriptors
     */
    fun close() {
        parcelFileDescriptor.close()
    }
}
