/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import emu.skyline.input.ButtonId

/**
 * This interface is used to allow proxying of [OnScreenButton]s
 */
interface ConfigurableButton {
    val buttonId : ButtonId
    val config : OnScreenConfiguration

    /**
     * Returns whether this button supports toggle mode
     * Usually true for buttons and false for sticks
     */
    fun supportsToggleMode() : Boolean = true

    /**
     * Starts a button move session
     * @param x The x coordinate of the initial touch
     * @param y The y coordinate of the initial touch
     */
    fun startMove(x : Float, y : Float)

    /**
     * Moves this button to the given coordinates
     */
    fun move(x : Float, y : Float)

    /**
     * Ends the current move session
     */
    fun endMove()

    /**
     * Resets the button to its default configuration
     */
    fun resetConfig()

    fun moveUp()
    fun moveDown()
    fun moveLeft()
    fun moveRight()
}
