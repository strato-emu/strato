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
import android.graphics.drawable.BitmapDrawable
import android.net.Uri
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.PreferenceManager
import emu.skyline.R
import emu.skyline.SkylineApplication
import emu.skyline.getPublicFilesDir
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream

class ProfilePicturePreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    init {
        layoutResource = R.layout.preference_profile_picture
    }

    private val skylineFilesDir = SkylineApplication.instance.getPublicFilesDir().canonicalPath
    private val profilePictureDir = "$skylineFilesDir/switch/nand/system/save/8000000000000010/su/avators"
    private val profilePicture = "$profilePictureDir/profile_picture.jpeg"
    private val pickMedia = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        try {
            if (uri != null) { // The user selected a picture
                PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, profilePicture).apply()
                File(profilePictureDir).mkdirs()
                context.applicationContext.contentResolver.let { contentResolver : ContentResolver ->
                    val readUriPermission : Int = Intent.FLAG_GRANT_READ_URI_PERMISSION
                    contentResolver.takePersistableUriPermission(uri, readUriPermission)
                    contentResolver.openInputStream(uri)?.use { inputStream : InputStream ->
                        var bitmap = BitmapFactory.decodeStream(inputStream)
                        // Compress the picture
                        bitmap = Bitmap.createScaledBitmap(bitmap, 256, 256, false)
                        storeBitmap(bitmap, profilePicture)
                    }
                }
            } else { // No picture was selected, clear the profile picture if one was already set
                if (File(profilePicture).exists()) {
                    File(profilePicture).delete()
                }
                PreferenceManager.getDefaultSharedPreferences(context).edit().putString(key, "No picture selected").apply()
            }
            updatePreview()
            notifyChanged()
        } catch (e : Exception) {
            e.printStackTrace()
        }
    }

    init {
        summaryProvider = SummaryProvider<ProfilePicturePreference> { preference ->
            var relativePath = Uri.decode(preference.getPersistedString("No picture selected"))
            if (relativePath.startsWith(skylineFilesDir))
                relativePath = relativePath.substring(skylineFilesDir.length)
            relativePath
        }
        updatePreview()
    }

    override fun onClick() = pickMedia.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))

    private fun updatePreview() {
        var drawable: BitmapDrawable? = null
        if (File(profilePicture).exists()) {
            val preview = BitmapFactory.decodeFile(profilePicture)
            if (preview != null) {
                drawable = BitmapDrawable(context.resources, preview)
            }
        }
        icon = drawable
    }

    /**
     * Given a bitmap, saves it in the specified location
     */
    private fun storeBitmap(bitmap : Bitmap, filePath : String) {
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
        } finally {
            bitmap.recycle()
        }
    }
}
