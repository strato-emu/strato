/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

/**
 * This enumerates all buttons on an NPad controller
 */
enum class NpadButton : ButtonId {
    A,
    B,
    X,
    Y,
    L3,
    R3,
    L,
    R,
    ZL,
    ZR,
    Plus,
    Minus,
    DpadLeft,
    DpadUp,
    DpadRight,
    DpadDown,
    LeftStickLeft,
    LeftStickUp,
    LeftStickRight,
    LeftStickDown,
    RightStickLeft,
    RightStickUp,
    RightStickRight,
    RightStickDown,
    LeftSL,
    LeftSR,
    RightSL,
    RightSR;

    /**
     * This just returns the value as setting the [ordinal]-th bit in a [Long]
     */
    override fun value() : Long {
        return (1.toLong()) shl ordinal
    }
}

/**
 * This enumerates all the axis on an NPad controller
 */
enum class NpadAxis {
    RX,
    RY,
    LX,
    LY,
}
