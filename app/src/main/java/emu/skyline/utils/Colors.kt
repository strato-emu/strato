/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utils

import android.graphics.Color

enum class SwitchColors(val color : Int) {
    GRAY(Color.GRAY),
    TRANSPARENT(Color.argb(180, 0, 0, 0)),
    WHITE(Color.argb(180, 255, 255, 255)),
    NEON_YELLOW(Color.argb(180, 230, 255, 0)),
    NEON_PURPLE(Color.argb(180, 180, 0, 230)),
    NEON_RED(Color.argb(180, 255, 60, 40)),
    MARIO_RED(Color.argb(180, 225, 15, 0)),
    NEON_BLUE(Color.argb(180, 10, 185, 230)),
    BLUE(Color.argb(180, 70, 85, 245)),
    NEON_GREEN(Color.argb(180, 30, 220, 0));

    companion object {
        val colors = SwitchColors.values().map { clr -> clr.color }
    }
}
