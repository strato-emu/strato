/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.utils

import android.graphics.PointF

fun PointF.add(p : PointF) = PointF(x, y).apply {
    x += p.x
    y += p.y
}

fun PointF.multiply(scalar : Float) = PointF(x, y).apply {
    x *= scalar
    y *= scalar
}

fun PointF.normalize() = multiply(1f / length())
