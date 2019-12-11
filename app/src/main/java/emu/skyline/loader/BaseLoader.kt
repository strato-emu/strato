package emu.skyline.loader

import android.content.ContentResolver
import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.provider.OpenableColumns
import androidx.core.graphics.drawable.toBitmap
import emu.skyline.R
import emu.skyline.utility.RandomAccessDocument
import java.io.IOException
import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.io.Serializable
import java.util.*

enum class TitleFormat {
    NRO, XCI, NSP
}

fun getTitleFormat(uri: Uri, contentResolver: ContentResolver): TitleFormat {
    var uriStr = ""
    contentResolver.query(uri, null, null, null, null)?.use { cursor ->
        val nameIndex: Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        cursor.moveToFirst()
        uriStr = cursor.getString(nameIndex)
    }
    return TitleFormat.valueOf(uriStr.substring(uriStr.lastIndexOf(".") + 1).toUpperCase(Locale.ROOT))
}

class TitleEntry(var name: String, var author: String, var romType: TitleFormat, var valid: Boolean, var uri: Uri, var icon: Bitmap) : Serializable {
    constructor(context: Context, author: String, romType: TitleFormat, valid: Boolean, uri: Uri) : this("", author, romType, valid, uri, context.resources.getDrawable(R.drawable.ic_missing, context.theme).toBitmap(256, 256)) {
        context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
            val nameIndex: Int = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            cursor.moveToFirst()
            name = cursor.getString(nameIndex)
        }
    }

    @Throws(IOException::class)
    private fun writeObject(output: ObjectOutputStream) {
        output.writeUTF(name)
        output.writeUTF(author)
        output.writeObject(romType)
        output.writeUTF(uri.toString())
        output.writeBoolean(valid)
        icon.compress(Bitmap.CompressFormat.WEBP, 100, output)
    }

    @Throws(IOException::class, ClassNotFoundException::class)
    private fun readObject(input: ObjectInputStream) {
        name = input.readUTF()
        author = input.readUTF()
        romType = input.readObject() as TitleFormat
        uri = Uri.parse(input.readUTF())
        valid = input.readBoolean()
        icon = BitmapFactory.decodeStream(input)
    }
}

internal abstract class BaseLoader(val context: Context, val romType: TitleFormat) {
    abstract fun getTitleEntry(file: RandomAccessDocument, uri: Uri): TitleEntry

    abstract fun verifyFile(file: RandomAccessDocument): Boolean
}
