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
import kotlin.math.roundToInt

open class CircularButton(
        onScreenControllerView : OnScreenControllerView,
        buttonId : ButtonId,
        defaultRelativeX : Float,
        defaultRelativeY : Float,
        defaultRelativeRadiusToX : Float,
        drawableId : Int = R.drawable.ic_button
) : OnScreenButton(
        onScreenControllerView,
        buttonId,
        defaultRelativeX,
        defaultRelativeY,
        defaultRelativeRadiusToX * 2f,
        defaultRelativeRadiusToX * CONFIGURED_ASPECT_RATIO * 2f,
        drawableId
) {
    val radius get() = itemWidth / 2f

    override fun isTouched(x : Float, y : Float) : Boolean = PointF(currentX, currentY).minus(PointF(x, y)).length() <= radius

    override fun onFingerDown(x : Float, y : Float) {
        drawable.alpha = (255 * 0.5f).roundToInt()
    }

    override fun onFingerUp(x : Float, y : Float) {
        drawable.alpha = 255
    }
}

class JoystickButton(
        onScreenControllerView : OnScreenControllerView,
        buttonId : ButtonId,
        defaultRelativeX : Float,
        defaultRelativeY : Float,
        defaultRelativeRadiusToX : Float
) : CircularButton(
        onScreenControllerView,
        buttonId,
        defaultRelativeX,
        defaultRelativeY,
        defaultRelativeRadiusToX,
        R.drawable.ic_stick_circle
) {

    private val innerButton = CircularButton(onScreenControllerView, buttonId, config.relativeX, config.relativeY, defaultRelativeRadiusToX * 0.75f, R.drawable.ic_stick)

    private var fingerDownTime = 0L
    private var fingerUpTime = 0L
    var shortDoubleTapped = false
        private set

    override fun renderCenteredText(canvas : Canvas, text : String, size : Float, x : Float, y : Float) = Unit

    override fun render(canvas : Canvas) {
        super.render(canvas)

        innerButton.width = width
        innerButton.height = height
        innerButton.render(canvas)
    }

    override fun onFingerDown(x : Float, y : Float) {
        relativeX = x / width
        relativeY = (y - heightDiff) / adjustedHeight
        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY

        val currentTime = SystemClock.elapsedRealtime()
        val firstTapDiff = fingerUpTime - fingerDownTime
        val secondTapDiff = currentTime - fingerUpTime
        if (firstTapDiff in 0..500 && secondTapDiff in 0..500) {
            shortDoubleTapped = true
        }
        fingerDownTime = currentTime
    }

    override fun onFingerUp(x : Float, y : Float) {
        loadConfigValues()
        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY

        fingerUpTime = SystemClock.elapsedRealtime()
        shortDoubleTapped = false
    }

    fun onFingerMoved(x : Float, y : Float) : PointF {
        val position = PointF(currentX, currentY)
        var finger = PointF(x, y)
        val outerToInner = finger.minus(position)
        val distance = outerToInner.length()
        if (distance >= radius) {
            finger = position.add(outerToInner.multiply(1f / distance * radius))
        }

        if (distance > radius * 0.075f) {
            fingerDownTime = 0
            fingerUpTime = 0
        }

        innerButton.relativeX = finger.x / width
        innerButton.relativeY = (finger.y - heightDiff) / adjustedHeight
        return finger.minus(position).multiply(1f / radius)
    }

    fun outerToInner() = PointF(innerButton.currentX, innerButton.currentY).minus(PointF(currentX, currentY))

    override fun edit(x : Float, y : Float) {
        super.edit(x, y)

        innerButton.relativeX = relativeX
        innerButton.relativeY = relativeY
    }

    override fun resetRelativeValues() {
        super.resetRelativeValues()

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
        drawableId : Int = R.drawable.ic_rectangular_button
) : OnScreenButton(
        onScreenControllerView,
        buttonId,
        defaultRelativeX,
        defaultRelativeY,
        defaultRelativeWidth,
        defaultRelativeHeight,
        drawableId
) {
    override fun isTouched(x : Float, y : Float) =
            currentBounds.contains(x.roundToInt(), y.roundToInt())

    override fun onFingerDown(x : Float, y : Float) {
        drawable.alpha = (255 * 0.5f).roundToInt()
    }

    override fun onFingerUp(x : Float, y : Float) {
        drawable.alpha = 255
    }
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
    val circularButtons = listOf(
            CircularButton(onScreenControllerView, A, 0.95f, 0.65f, 0.025f),
            CircularButton(onScreenControllerView, B, 0.9f, 0.75f, 0.025f),
            CircularButton(onScreenControllerView, X, 0.9f, 0.55f, 0.025f),
            CircularButton(onScreenControllerView, Y, 0.85f, 0.65f, 0.025f),
            CircularButton(onScreenControllerView, DpadLeft, 0.2f, 0.65f, 0.025f),
            CircularButton(onScreenControllerView, DpadUp, 0.25f, 0.55f, 0.025f),
            CircularButton(onScreenControllerView, DpadRight, 0.3f, 0.65f, 0.025f),
            CircularButton(onScreenControllerView, DpadDown, 0.25f, 0.75f, 0.025f),
            CircularButton(onScreenControllerView, Plus, 0.57f, 0.75f, 0.025f),
            CircularButton(onScreenControllerView, Minus, 0.43f, 0.75f, 0.025f),
            CircularButton(onScreenControllerView, Menu, 0.5f, 0.75f, 0.025f)
    )

    val joysticks = listOf(
            JoystickButton(onScreenControllerView, LeftStick, 0.1f, 0.8f, 0.05f),
            JoystickButton(onScreenControllerView, RightStick, 0.75f, 0.6f, 0.05f)
    )

    val rectangularButtons = listOf(
            RectangularButton(onScreenControllerView, L, 0.1f, 0.25f, 0.075f, 0.08f),
            RectangularButton(onScreenControllerView, R, 0.9f, 0.25f, 0.075f, 0.08f)
    )

    val triggerButtons = listOf(
            TriggerButton(onScreenControllerView, ZL, 0.1f, 0.1f, 0.075f, 0.08f),
            TriggerButton(onScreenControllerView, ZR, 0.9f, 0.1f, 0.075f, 0.08f)
    )

    /**
     * We can take any of the global scale variables from the buttons
     */
    var globalScale
        get() = circularButtons[0].config.globalScale
        set(value) {
            circularButtons[0].config.globalScale = value
        }
}
