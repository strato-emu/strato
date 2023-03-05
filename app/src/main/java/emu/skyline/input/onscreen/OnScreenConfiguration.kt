/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.content.Context
import emu.skyline.input.ButtonId
import emu.skyline.utils.SwitchColors
import emu.skyline.utils.sharedPreferences

class OnScreenConfiguration(private val context : Context, private val buttonId : ButtonId, defaultRelativeX : Float, defaultRelativeY : Float) {
    private inline fun <reified T> config(default : T, prefix : String = "${buttonId.name}_") = sharedPreferences(context, default, prefix, "controller_config")

    var alpha by config(155, "")
    var textColor by config(SwitchColors.BLACK.color)
    var backgroundColor by config(SwitchColors.WHITE.color)
    var enabled by config(true)
    var globalScale by config(1.15f, "")
    var relativeX by config(defaultRelativeX)
    var relativeY by config(defaultRelativeY)
}
