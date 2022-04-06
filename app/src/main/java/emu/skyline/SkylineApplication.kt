/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.app.Application
import android.content.Context
import dagger.hilt.android.HiltAndroidApp
import emu.skyline.di.getSettings
import java.io.File

/**
 * @return The optimal directory for putting public files inside, this may return a private directory if a public directory cannot be retrieved
 */
fun Context.getPublicFilesDir() : File = getExternalFilesDir(null) ?: filesDir

@HiltAndroidApp
class SkylineApplication : Application() {
    private external fun initializeLog(appFilesPath : String, logLevel : Int)

    override fun onCreate() {
        super.onCreate()
        System.loadLibrary("skyline")

        val appFilesPath = applicationContext.getPublicFilesDir().canonicalPath
        File("$appFilesPath/logs/").mkdirs()
        initializeLog("$appFilesPath/", getSettings().logLevel.toInt())
    }
}
