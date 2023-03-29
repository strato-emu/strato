/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.graphics.Canvas
import android.graphics.PointF
import android.os.SystemClock
import androidx.core.graphics.minus
import emu.skyline.R
import emu.skyline.input.ButtonId
import emu.skyline.input.ButtonId.*
import emu.skyline.input.StickId
import emu.skyline.input.StickId.Left
import emu.skyline.input.StickId.Right
import emu.skyline.utils.add
import emu.skyline.utils.multiply
import kotlin.math.roundToInt

open class CircularButton(
    onScreenControllerView : OnScreenControllerView,
    buttonId : ButtonId,
    defaultRelativeX : Float,
    defaultRelativeY : Float,
    defaultRelativeRadiusToX : Float,
    drawableId : Int = R.drawable.ic_button,
    defaultEnabled : Boolean = true
) : OnScreenButton(
    onScreenControllerView,
    buttonId,
    defaultRelativeX,
    defaultRelativeY,
    defaultRelativeRadiusToX * 2f,
    defaultRelativeRadiusToX * CONFIGURED_ASPECT_RATIO * 2f,
    drawableId,
    defaultEnabled
) {
    val radius get() = itemWidth / 2f

    /**
     * Checks if [x] and [y] are within circle
     */
    override fun isTouched(x : Float, y : Float) : Boolean = (PointF(currentX, currentY) - (PointF(x, y))).length() <= radius
}

class JoystickButton(
    onScreenControllerView : OnScreenControllerView,
    val stickId : StickId,
    defaultRelativeX : Float,
    defaultRelativeY : Float,
    defaultRelativeRadiusToX : Float
) : CircularButton(
    onScreenControllerView,
    stickId.button,
    defaultRelativeX,
    defaultRelativeY,
    defaultRelativeRadiusToX,
    R.drawable.ic_button
) {
    private val innerButton = CircularButton(onScreenControllerView, buttonId, config.relativeX, config.relativeY, defaultRelativeRadiusToX * 0.75f, R.drawable.ic_stick)

    var recenterSticks = false
    private var initialTapPosition = PointF()
    private var fingerDownTime = 0L
    private var fingerUpTime = 0L
    var shortDoubleTapped = false
        private set

    override fun renderCenteredText(canvas : Canvas, text : String, size : Float, x : Float, y : Float, alpha : Int) = Unit

    override fun render(canvas : Canvas) {
        super.render(canvas)

        innerButton.width = width
        innerButton.height = height
        innerButton.render(canvas)
    }

    override fun onFingerDown(x : Float, y : Float) {
        val relativeX = x / width
        val relativeY = (y - heightDiff) / adjustedHeight
        if (recenterSticks) {
            this.relativeX = relativeX
            this.relativeY = relativeY
        }
        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY

        // If first and second tap occur within 500 mills, then trigger stick press
        val currentTime = SystemClock.elapsedRealtime()
        initialTapPosition = PointF(x, y)
        val firstTapDiff = fingerUpTime - fingerDownTime
        val secondTapDiff = currentTime - fingerUpTime
        if (firstTapDiff in 0..500 && secondTapDiff in 0..500) {
            shortDoubleTapped = true
            // Indicate stick being pressed with lower alpha value
            isPressed = true
        }
        fingerDownTime = currentTime
    }

    override fun onFingerUp(x : Float, y : Float) {
        loadConfigValues()
        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY

        fingerUpTime = SystemClock.elapsedRealtime()
        shortDoubleTapped = false
        isPressed = false
    }

    fun onFingerMoved(x : Float, y : Float, manualMove : Boolean = true) : PointF {
        val position = PointF(currentX, currentY)
        var finger = PointF(x, y)
        val outerToInner = finger.minus(position)
        val distance = outerToInner.length()
        if (distance > radius) {
            // Limit distance to radius
            finger = position.add(outerToInner.multiply(1f / distance * radius))
        }

        // If finger get moved to much, then don't trigger as joystick being pressed
        if (manualMove && initialTapPosition.minus(finger).length() > radius * 0.075f) {
            fingerDownTime = 0
            fingerUpTime = 0
        }

        innerButton.relativeX = finger.x / width
        innerButton.relativeY = (finger.y - heightDiff) / adjustedHeight
        return finger.minus(position).multiply(1f / radius)
    }

    fun outerToInner() = PointF(innerButton.currentX, innerButton.currentY).minus(PointF(currentX, currentY))

    fun outerToInnerRelative() = outerToInner().multiply(1f / radius)

    override fun move(x : Float, y : Float) {
        super.move(x, y)

        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY
    }

    override fun resetConfig() {
        super.resetConfig()

        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY
    }
}

open class RectangularButton(
    onScreenControllerView : OnScreenControllerView,
    buttonId : ButtonId,
    defaultRelativeX : Float,
    defaultRelativeY : Float,
    defaultRelativeWidth : Float,
    defaultRelativeHeight : Float,
    drawableId : Int = R.drawable.ic_rectangular_button,
    defaultEnabled : Boolean = true
) : OnScreenButton(
    onScreenControllerView,
    buttonId,
    defaultRelativeX,
    defaultRelativeY,
    defaultRelativeWidth,
    defaultRelativeHeight,
    drawableId,
    defaultEnabled
) {
    override fun isTouched(x : Float, y : Float) = currentBounds.contains(x.roundToInt(), y.roundToInt())
}

class TriggerButton(
    onScreenControllerView : OnScreenControllerView,
    buttonId : ButtonId,
    defaultRelativeX : Float,
    defaultRelativeY : Float,
    defaultRelativeWidth : Float,
    defaultRelativeHeight : Float
) : RectangularButton(
    onScreenControllerView,
    buttonId,
    defaultRelativeX,
    defaultRelativeY,
    defaultRelativeWidth,
    defaultRelativeHeight,
    when (buttonId) {
        ZL -> R.drawable.ic_trigger_button_left

        ZR -> R.drawable.ic_trigger_button_right

        else -> error("Unsupported trigger button")
    }
)

class Controls(onScreenControllerView : OnScreenControllerView) {
    private val buttonA = CircularButton(onScreenControllerView, A, 0.81f, 0.73f, 0.025f)
    private val buttonB = CircularButton(onScreenControllerView, B, 0.76f, 0.85f, 0.025f)
    private val buttonX = CircularButton(onScreenControllerView, X, 0.76f, 0.61f, 0.025f)
    private val buttonY = CircularButton(onScreenControllerView, Y, 0.71f, 0.73f, 0.025f)

    private val buttonDpadLeft = CircularButton(onScreenControllerView, DpadLeft, 0.06f, 0.53f, 0.025f)
    private val buttonDpadUp = CircularButton(onScreenControllerView, DpadUp, 0.11f, 0.41f, 0.025f)
    private val buttonDpadRight = CircularButton(onScreenControllerView, DpadRight, 0.16f, 0.53f, 0.025f)
    private val buttonDpadDown = CircularButton(onScreenControllerView, DpadDown, 0.11f, 0.65f, 0.025f)

    private val buttonL = RectangularButton(onScreenControllerView, L, 0.1f, 0.22f, 0.09f, 0.1f)
    private val buttonR = RectangularButton(onScreenControllerView, R, 0.9f, 0.22f, 0.09f, 0.1f)

    private val buttonZL = TriggerButton(onScreenControllerView, ZL, 0.1f, 0.08f, 0.09f, 0.1f)
    private val buttonZR = TriggerButton(onScreenControllerView, ZR, 0.9f, 0.08f, 0.09f, 0.1f)

    private val buttonL3 = CircularButton(onScreenControllerView, L3, 0.12f, 0.87f, 0.025f, defaultEnabled = false)
    private val buttonR3 = CircularButton(onScreenControllerView, R3, 0.88f, 0.87f, 0.025f, defaultEnabled = false)

    private val circularButtonPairs = listOf(setOf(buttonA, buttonB, buttonX, buttonY), setOf(buttonDpadLeft, buttonDpadUp, buttonDpadRight, buttonDpadDown))

    private val triggerButtonPairs = listOf(setOf(buttonL, buttonZL), setOf(buttonR, buttonZR))

    private val stickButtons = setOf(buttonL3, buttonR3)

    val buttonPairs = circularButtonPairs + triggerButtonPairs

    val circularButtons = circularButtonPairs.flatten() + stickButtons + listOf(
        CircularButton(onScreenControllerView, Plus, 0.57f, 0.85f, 0.025f),
        CircularButton(onScreenControllerView, Minus, 0.43f, 0.85f, 0.025f),
        CircularButton(onScreenControllerView, Menu, 0.5f, 0.85f, 0.025f)
    )

    val joysticks = listOf(
        JoystickButton(onScreenControllerView, Left, 0.24f, 0.75f, 0.053f),
        JoystickButton(onScreenControllerView, Right, 0.9f, 0.53f, 0.053f)
    )

    val rectangularButtons = listOf(buttonL, buttonR)

    val triggerButtons = listOf(buttonZL, buttonZR)

    val allButtons = circularButtons + joysticks + rectangularButtons + triggerButtons

    /**
     * We can take any of the global text color variables from the buttons
     */
    val globalTextColor
        get() = circularButtons.first().config.textColor

    /**
     * We can take any of the global background color variables from the buttons
     */
    val globalBackgroundColor
        get() = circularButtons.first().config.backgroundColor
}
