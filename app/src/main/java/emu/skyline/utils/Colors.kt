/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.graphics.Color

enum class SwitchColors(val color : Int) {
    GRAY(Color.GRAY),
    BLACK(Color.rgb(0, 0, 0)),
    WHITE(Color.rgb(255, 255, 255)),
    NEON_YELLOW(Color.rgb(230, 255, 0)),
    NEON_PURPLE(Color.rgb(180, 0, 230)),
    NEON_RED(Color.rgb(255, 60, 40)),
    MARIO_RED(Color.rgb(225, 15, 0)),
    NEON_BLUE(Color.rgb(10, 185, 230)),
    BLUE(Color.rgb(70, 85, 245)),
    NEON_GREEN(Color.rgb(30, 220, 0));

    companion object {
        val colors get() = SwitchColors.values().map { clr -> clr.color }
    }
}
