/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.views

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View

/**
 * A view that draws a grid, used for aligning on-screen controller buttons
 */
class AlignmentGridView @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = 0) : View(context, attrs, defStyleAttr) {
    var gridSize = 16
        set(value) {
            field = value
            invalidate()
        }

    private val gridPaint = Paint().apply {
        color = Color.WHITE
        alpha = 135
    }
    private val gridCenterPaint = Paint().apply {
        color = Color.WHITE
        alpha = 55
        strokeWidth = 10f
    }

    override fun onDraw(canvas : Canvas) {
        super.onDraw(canvas)
        drawGrid(canvas)
    }

    /**
     * Draws a centered grid on the given canvas
     */
    private fun drawGrid(canvas : Canvas) {
        val centerX = width / 2f
        val centerY = height / 2f
        val gridSize = gridSize
        // Compute the coordinates of the first grid line for each axis, because we want a centered grid, which might not start at [0,0]
        val startX = centerX % gridSize
        val startY = centerY % gridSize

        // Draw the center lines with a thicker stroke
        canvas.drawLine(centerX, 0f, centerX, height.toFloat(), gridCenterPaint)
        canvas.drawLine(0f, centerY, width.toFloat(), centerY, gridCenterPaint)

        // Draw the rest of the grid starting from the start coordinates
        // Draw vertical lines
        for (i in 0..width step gridSize) {
            canvas.drawLine(startX + i, 0f, startX + i, height.toFloat(), gridPaint)
        }
        // Draw horizontal lines
        for (i in 0..height step gridSize) {
            canvas.drawLine(0f, startY + i, width.toFloat(), startY + i, gridPaint)
        }
    }
}
