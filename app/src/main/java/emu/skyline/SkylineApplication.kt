/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.app.Application
import dagger.hilt.android.HiltAndroidApp
import emu.skyline.di.getSettings

@HiltAndroidApp
class SkylineApplication : Application() {
    private external fun initializeLog(appFilesPath : String, logLevel : Int)

    override fun onCreate() {
        super.onCreate()
        System.loadLibrary("skyline")
        initializeLog(applicationContext.filesDir.canonicalPath + "/", getSettings().logLevel.toInt())
    }
}
