/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.content.Context
import emu.skyline.input.ButtonId
import emu.skyline.utils.SwitchColors
import emu.skyline.utils.sharedPreferences

interface OnScreenConfiguration {
    companion object {
        const val GroupDisabled = 0
        const val GroupEnabled = 1
        const val GroupIndeterminate = 2

        const val DefaultToggleMode = false

        const val MinAlpha = 0
        const val MaxAlpha = 255
        const val DefaultAlpha = 128

        const val MinScale = 0.5f
        const val MaxScale = 2.5f
        const val DefaultScale = 1f

        const val MinActivationRadius = 1.0f
        const val DefaultActivationRadius = 1.0f
        const val MaxActivationRadius = 8.0f

        val DefaultTextColor = SwitchColors.BLACK.color
        val DefaultBackgroundColor = SwitchColors.WHITE.color
    }

    var enabled : Boolean

    /**
     * The state of a group of buttons, returns an integer that can be used to set the state of a MaterialCheckBox
     */
    val groupEnabled get() = if (enabled) GroupEnabled else GroupDisabled

    var toggleMode : Boolean

    /**
     * The toggle mode of group of buttons, returns an integer that can be used to set the state of a MaterialCheckBox
     */
    val groupToggleMode get() = if (toggleMode) GroupEnabled else GroupDisabled

    var alpha : Int
    var textColor : Int
    var backgroundColor : Int

    var scale : Float
    var relativeX : Float
    var relativeY : Float

    /**
     * A modifier applied to the radius of the stick, applied when determining when the stick has been touched
     */
    var activationRadius : Float
}

class OnScreenConfigurationImpl(private val context : Context, private val buttonId : ButtonId, defaultRelativeX : Float, defaultRelativeY : Float, defaultEnabled : Boolean) : OnScreenConfiguration {
    private inline fun <reified T> config(default : T, prefix : String = "${buttonId.name}_") = sharedPreferences(context, default, prefix, "controller_config")

    override var enabled by config(defaultEnabled)
    override var toggleMode by config(OnScreenConfiguration.DefaultToggleMode)

    override var alpha by config(OnScreenConfiguration.DefaultAlpha)
    override var textColor by config(OnScreenConfiguration.DefaultTextColor)
    override var backgroundColor by config(OnScreenConfiguration.DefaultBackgroundColor)

    override var scale by config(OnScreenConfiguration.DefaultScale)
    override var relativeX by config(defaultRelativeX)
    override var relativeY by config(defaultRelativeY)

    override var activationRadius by config(OnScreenConfiguration.DefaultActivationRadius)
}
