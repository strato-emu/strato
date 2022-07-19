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

    init {
        instance = this
    }

    companion object {
        lateinit var instance : SkylineApplication
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        System.loadLibrary("skyline")

        val publicAppFilesPath = applicationContext.getPublicFilesDir().canonicalPath
        File("$publicAppFilesPath/logs/").mkdirs()
        initializeLog("$publicAppFilesPath/", getSettings().logLevel)
    }
}
