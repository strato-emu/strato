/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.PreferenceManager
import androidx.preference.R
import emu.skyline.SkylineApplication
import emu.skyline.getPublicFilesDir
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

class ProfilePicturePreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val pickMedia = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        val profilePictureDir = SkylineApplication.instance.getPublicFilesDir().canonicalPath + "/switch/nand/system/save/8000000000000010/su/avators"
        val profilePictureName = "profile_picture.jpeg"
        try {
            if (uri != null) { // The user selected a picture
                PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, "$profilePictureDir/$profilePictureName").apply()
                File(profilePictureDir).mkdirs()
                context.applicationContext.contentResolver.let { contentResolver : ContentResolver ->
                    val readUriPermission : Int = Intent.FLAG_GRANT_READ_URI_PERMISSION
                    contentResolver.takePersistableUriPermission(uri, readUriPermission)
                    contentResolver.openInputStream(uri)?.use { inputStream : InputStream ->
                        var bitmap = BitmapFactory.decodeStream(inputStream)
                        // Compress the picture
                        bitmap = Bitmap.createScaledBitmap(bitmap, 256, 256, false)
                        StoreBitmap(bitmap, "$profilePictureDir/$profilePictureName")
                    }
                }
            } else { // No picture was selected, clear the profile picture if one was already set
                if (File("$profilePictureDir/$profilePictureName").exists()) {
                    File("$profilePictureDir/$profilePictureName").delete()
                }
                PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, "No picture selected").apply()
            }
            notifyChanged()
        } catch (e : Exception) {
            e.printStackTrace()
        }
    }

    init {
        summaryProvider = SummaryProvider<ProfilePicturePreference> { preference ->
            Uri.decode(preference.getPersistedString("No picture selected"))
        }
    }

    override fun onClick() = pickMedia.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))

    /**
     * Given a bitmap, saves it in the specified location
     */
    private fun StoreBitmap(bitmap : Bitmap, filePath : String) {
        try {
            // Create the file where the bitmap will be stored
            val file = File(filePath)
            file.createNewFile()
            // Store bitmap as JPEG
            val outputFile = FileOutputStream(file)
            bitmap.compress(Bitmap.CompressFormat.JPEG, 100, outputFile)
            outputFile.flush()
            outputFile.close()
        } catch (e : Exception) {
            e.printStackTrace()
        }
    }
}
