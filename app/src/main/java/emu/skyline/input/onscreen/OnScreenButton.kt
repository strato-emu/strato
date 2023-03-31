/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.PointF
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
    final override val buttonId : ButtonId,
    private val defaultRelativeX : Float,
    private val defaultRelativeY : Float,
    private val defaultRelativeWidth : Float,
    private val defaultRelativeHeight : Float,
    drawableId : Int,
    private val defaultEnabled : Boolean
) : ConfigurableButton {
    companion object {
        /**
         * Aspect ratio the default values were based on
         */
        const val CONFIGURED_ASPECT_RATIO = 2074f / 874f
    }

    final override val config = OnScreenConfigurationImpl(onScreenControllerView.context, buttonId, defaultRelativeX, defaultRelativeY, defaultEnabled)

    protected val drawable = ContextCompat.getDrawable(onScreenControllerView.context, drawableId)!!

    private val buttonSymbolPaint = Paint().apply {
        color = config.textColor
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        isAntiAlias = true
    }
    private val textBoundsRect = Rect()

    var relativeX = config.relativeX
    var relativeY = config.relativeY
    private val relativeWidth get() = defaultRelativeWidth * config.scale
    private val relativeHeight get() = defaultRelativeHeight * config.scale

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

    val currentBounds get() = Rect(left.roundToInt(), top.roundToInt(), (left + itemWidth).roundToInt(), (top + itemHeight).roundToInt())

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
    protected val editInfo = onScreenControllerView.editInfo

    /**
     * The touch point when the edit session started
     */
    protected val editInitialTouchPoint = PointF()

    /**
     * The scale of the button when the edit session started
     */
    private var editInitialScale = 0f

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
        val alpha = if (isPressed) config.alpha / 3 else config.alpha
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

    fun saveConfigValues() {
        config.relativeX = relativeX
        config.relativeY = relativeY
    }

    override fun startMove(x : Float, y : Float) {
        editInitialTouchPoint.set(x, y)
        editInitialScale = config.scale
    }

    override fun move(x : Float, y : Float) {
        var adjustedX = x
        var adjustedY = y

        if (editInfo.snapToGrid) {
            val centerX = width / 2f
            val centerY = height / 2f
            val gridSize = editInfo.gridSize
            // The coordinates of the first grid line for each axis, because the grid is centered and might not start at [0,0]
            val startX = centerX % gridSize
            val startY = centerY % gridSize

            /**
             * The offset to apply to a coordinate to snap it to the grid is the remainder of
             * the coordinate divided by the grid size.
             * Since the grid is centered on the screen and might not start at [0,0] we need to
             * subtract the grid start offset, otherwise we'd be calculating the offset for a grid that starts at [0,0].
             *
             * Example: Touch event X: 158 | Grid size: 50 | Grid start X: 40 -> Grid lines at 40, 90, 140, 190, ...
             * Snap offset: 158 - 40 = 118 -> 118 % 50 = 18
             * Apply offset to X: 158 - 18 = 140 which is a grid line
             *
             * If we didn't subtract the grid start offset:
             * Snap offset: 158 % 50 = 8
             * Apply offset to X: 158 - 8 = 150 which is not a grid line
             */
            val snapOffsetX = (x - startX) % gridSize
            val snapOffsetY = (y - startY) % gridSize

            adjustedX = x - snapOffsetX
            adjustedY = y - snapOffsetY
        }

        relativeX = adjustedX / width
        relativeY = (adjustedY - heightDiff) / adjustedHeight
    }

    override fun endMove() {
        saveConfigValues()
    }

    /**
     * Returns the amount to move the button when the user presses an arrow key
     * @param gridOffset The offset to apply to the grid size so that the button always ends up on a grid line
     */
    private fun getMoveAmount(gridOffset : Int) : Int {
        return if (editInfo.snapToGrid)
            editInfo.gridSize + gridOffset
        else
            OnScreenEditInfo.ArrowKeyMoveAmount
    }

    override fun moveUp() {
        move(currentX, currentY - getMoveAmount(-1))
        saveConfigValues()
    }

    override fun moveDown() {
        move(currentX, currentY + getMoveAmount(+1))
        saveConfigValues()
    }

    override fun moveLeft() {
        move(currentX - getMoveAmount(-1), currentY)
        saveConfigValues()
    }

    override fun moveRight() {
        move(currentX + getMoveAmount(+1), currentY)
        saveConfigValues()
    }

    override fun resetConfig() {
        config.enabled = defaultEnabled
        config.alpha = OnScreenConfiguration.DefaultAlpha
        config.textColor = OnScreenConfiguration.DefaultTextColor
        config.backgroundColor = OnScreenConfiguration.DefaultBackgroundColor
        config.scale = OnScreenConfiguration.DefaultScale
        config.relativeX = defaultRelativeX
        config.relativeY = defaultRelativeY

        relativeX = defaultRelativeX
        relativeY = defaultRelativeY
    }
}
