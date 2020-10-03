/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.app.Application
import emu.skyline.input.InputManager

class SkylineApplication : Application() {
    override fun onCreate() {
        super.onCreate()
        InputManager.init(applicationContext)
    }
}