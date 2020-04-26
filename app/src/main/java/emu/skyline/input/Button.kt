/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

/**
 * This is a generic interface for all Button classes to implement
 */
interface ButtonId {
    /**
     * This should return the value of the Button according to what libskyline expects
     */
    fun value() : Long
}

enum class ButtonState(val state : Boolean) {
    Released(false),
    Pressed(true),
}
