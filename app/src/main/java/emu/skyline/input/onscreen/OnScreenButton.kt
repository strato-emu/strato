/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.LayerDrawable
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

    val config = OnScreenConfiguration(onScreenControllerView.context, buttonId, defaultRelativeX, defaultRelativeY)

    protected val drawable = ContextCompat.getDrawable(onScreenControllerView.context, drawableId)!!

    private val buttonSymbolPaint = Paint().apply {
        color = config.textColor
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        isAntiAlias = true
    }
    private val textBoundsRect = Rect()

    var relativeX = config.relativeX
    var relativeY = config.relativeY
    private val relativeWidth get() = defaultRelativeWidth * config.globalScale
    private val relativeHeight get() = defaultRelativeHeight * config.globalScale

    /**
     * The width of the view this button is in, populated by the view during draw
     */
    var width = 0

    /**
     * The height of the view this button is in, populated by the view during draw
     */
    var height = 0

    /**
     * The height of the view this button is in, adjusted to an arbitrary aspect ratio used during configuration
     */
    protected val adjustedHeight get() = width / CONFIGURED_ASPECT_RATIO

    /**
     * The difference between the view height and the adjusted view height.
     * This is used to offset the buttons to the bottom of the screen when the view height is larger than [adjustedHeight]
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

    var isPressed = false

    var hapticFeedback = false

    /**
     * The edit session information, populated by the view
     */
    protected var editInfo = onScreenControllerView.editInfo

    protected open fun renderCenteredText(canvas : Canvas, text : String, size : Float, x : Float, y : Float, alpha : Int) {
        buttonSymbolPaint.apply {
            textSize = size
            textAlign = Paint.Align.LEFT
            color = config.textColor
            this.alpha = alpha
            getTextBounds(text, 0, text.length, textBoundsRect)
        }
        canvas.drawText(text, x - textBoundsRect.width() / 2f - textBoundsRect.left, y + textBoundsRect.height() / 2f - textBoundsRect.bottom, buttonSymbolPaint)
    }

    open fun render(canvas : Canvas) {
        val bounds = currentBounds
        val alpha = if (isPressed) (config.alpha - 130).coerceIn(30..255) else config.alpha
        renderColors(drawable)
        drawable.apply {
            this.bounds = bounds
            this.alpha = alpha
            draw(canvas)
        }
        renderCenteredText(canvas, buttonId.short!!, itemWidth.coerceAtMost(itemHeight) * 0.4f, bounds.centerX().toFloat(), bounds.centerY().toFloat(), alpha)
    }

    private fun renderColors(drawable : Drawable) {
        when (drawable) {
            is GradientDrawable -> drawable.setColor(config.backgroundColor)
            is LayerDrawable -> {
                for (i in 0 until drawable.numberOfLayers) renderColors(drawable.getDrawable(i))
            }
            else -> drawable.setTint(config.backgroundColor)
        }
    }

    abstract fun isTouched(x : Float, y : Float) : Boolean

    open fun onFingerDown(x : Float, y : Float) {
        isPressed = true
    }

    open fun onFingerUp(x : Float, y : Float) {
        isPressed = false
    }

    fun loadConfigValues() {
        relativeX = config.relativeX
        relativeY = config.relativeY
    }

    /**
     * Starts an edit session
     * @param x The x coordinate of the initial touch
     * @param y The y coordinate of the initial touch
     */
    open fun startEdit(x : Float, y : Float) {
    }

    open fun edit(x : Float, y : Float) {
        when (editInfo.editMode) {
            EditMode.Move -> move(x, y)
            else -> return
        }
    }

    /**
     * Moves this button to the given coordinates
     */
    open fun move(x : Float, y : Float) {
        relativeX = x / width
        relativeY = (y - heightDiff) / adjustedHeight
    }

    /**
     * Ends the current edit session
     */
    open fun endEdit() {
        config.relativeX = relativeX
        config.relativeY = relativeY
    }

    open fun resetRelativeValues() {
        config.relativeX = defaultRelativeX
        config.relativeY = defaultRelativeY

        relativeX = defaultRelativeX
        relativeY = defaultRelativeY
    }

    open fun resetConfig() {
        resetRelativeValues()
        config.enabled = true
    }
}
