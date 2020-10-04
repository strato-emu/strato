/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import java.io.File
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable

fun <T : Serializable> ArrayList<T>.serialize(file : File) {
    ObjectOutputStream(file.outputStream()).use {
        it.writeObject(this)
    }
}

@Suppress("UNCHECKED_CAST")
fun <T : Serializable> loadSerializedList(file : File) = ObjectInputStream(file.inputStream()).use {
    it.readObject()
} as ArrayList<T>
