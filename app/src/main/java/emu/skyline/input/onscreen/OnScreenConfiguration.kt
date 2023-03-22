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
    companion object {
        const val DefaultAlpha = 130
        const val DefaultGlobalScale = 1.15f
        const val DefaultScale = 0.0f
    }

    private inline fun <reified T> config(default : T, prefix : String = "${buttonId.name}_") = sharedPreferences(context, default, prefix, "controller_config")

    var enabled by config(true)

    var alpha by config(DefaultAlpha, "")
    var textColor by config(SwitchColors.BLACK.color)
    var backgroundColor by config(SwitchColors.WHITE.color)

    /**
     * The global scale applied to all buttons
     */
    var globalScale by config(DefaultGlobalScale, "")

    /**
     * The scale of each button, this is added to the global scale
     * Allows buttons to have their own size, while still be controlled by the global scale
     */
    var scale by config(DefaultScale)

    var relativeX by config(defaultRelativeX)
    var relativeY by config(defaultRelativeY)
}
