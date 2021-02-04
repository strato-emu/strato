/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import androidx.core.content.ContextCompat
import emu.skyline.input.ButtonId
import kotlin.math.roundToInt

/**
 * Converts relative values, such as coordinates and boundaries, to their absolute counterparts, also handles layout modifications like scaling and custom positioning
 */
abstract class OnScreenButton(
        onScreenControllerView : OnScreenControllerView,
        val buttonId : ButtonId,
        private val defaultRelativeX : Float,
        private val defaultRelativeY : Float,
        private val defaultRelativeWidth : Float,
        private val defaultRelativeHeight : Float,
        drawableId : Int
) {
    companion object {
        /**
         * Aspect ratio the default values were based on
         */
        const val CONFIGURED_ASPECT_RATIO = 2074f / 874f
    }

    val config = if (onScreenControllerView.isInEditMode) ControllerConfigurationDummy(defaultRelativeX, defaultRelativeY)
    else ControllerConfigurationImpl(onScreenControllerView.context, buttonId, defaultRelativeX, defaultRelativeY)

    protected val drawable = ContextCompat.getDrawable(onScreenControllerView.context, drawableId)!!

    private val buttonSymbolPaint = Paint().apply { color = Color.GRAY }
    private val textBoundsRect = Rect()

    var relativeX = config.relativeX
    var relativeY = config.relativeY
    private val relativeWidth get() = defaultRelativeWidth * config.globalScale
    private val relativeHeight get() = defaultRelativeHeight * config.globalScale

    var width = 0
    var height = 0

    protected val adjustedHeight get() = width / CONFIGURED_ASPECT_RATIO

    /**
     * Buttons should be at bottom when device have large height than [adjustedHeight]
     */
    protected val heightDiff get() = (height - adjustedHeight).coerceAtLeast(0f)

    protected val itemWidth get() = width * relativeWidth
    private val itemHeight get() = adjustedHeight * relativeHeight

    val currentX get() = width * relativeX
    val currentY get() = adjustedHeight * relativeY + heightDiff

    private val left get() = currentX - itemWidth / 2f
    private val top get() = currentY - itemHeight / 2f

    protected val currentBounds get() = Rect(left.roundToInt(), top.roundToInt(), (left + itemWidth).roundToInt(), (top + itemHeight).roundToInt())

    /**
     * Keeps track of finger when there are multiple touches
     */
    var touchPointerId = -1
    var partnerPointerId = -1

    var isEditing = false
        private set

    protected open fun renderCenteredText(canvas : Canvas, text : String, size : Float, x : Float, y : Float) {
        buttonSymbolPaint.apply {
            textSize = size
            textAlign = Paint.Align.LEFT
            getTextBounds(text, 0, text.length, textBoundsRect)
        }
        canvas.drawText(text, x - textBoundsRect.width() / 2f - textBoundsRect.left, y + textBoundsRect.height() / 2f - textBoundsRect.bottom, buttonSymbolPaint)
    }

    open fun render(canvas : Canvas) {
        val bounds = currentBounds
        drawable.apply {
            this.bounds = bounds
            draw(canvas)
        }

        renderCenteredText(canvas, buttonId.short!!, itemWidth.coerceAtMost(itemHeight) * 0.4f, bounds.centerX().toFloat(), bounds.centerY().toFloat())
    }

    abstract fun isTouched(x : Float, y : Float) : Boolean

    abstract fun onFingerDown(x : Float, y : Float)

    abstract fun onFingerUp(x : Float, y : Float)

    fun loadConfigValues() {
        relativeX = config.relativeX
        relativeY = config.relativeY
    }

    fun startEdit() {
        isEditing = true
    }

    open fun edit(x : Float, y : Float) {
        relativeX = x / width
        relativeY = (y - heightDiff) / adjustedHeight
    }

    fun endEdit() {
        config.relativeX = relativeX
        config.relativeY = relativeY
        isEditing = false
    }

    open fun resetRelativeValues() {
        config.relativeX = defaultRelativeX
        config.relativeY = defaultRelativeY

        relativeX = defaultRelativeX
        relativeY = defaultRelativeY
    }
}
