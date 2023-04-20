/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.onscreen

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.PointF
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.AttributeSet
import android.util.TypedValue
import android.view.MotionEvent
import android.view.View
import android.view.View.OnTouchListener
import androidx.annotation.IntRange
import androidx.core.content.res.use
import emu.skyline.R
import emu.skyline.input.ButtonId
import emu.skyline.input.ButtonState
import emu.skyline.input.ControllerType
import emu.skyline.input.StickId
import emu.skyline.utils.add
import emu.skyline.utils.multiply
import emu.skyline.utils.normalize
import kotlin.math.roundToLong

typealias OnButtonStateChangedListener = (buttonId : ButtonId, state : ButtonState) -> Unit
typealias OnStickStateChangedListener = (stickId : StickId, position : PointF) -> Unit

/**
 * Renders On-Screen Controls as a single view, handles touch inputs and button toggling
 */
class OnScreenControllerView @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = 0, defStyleRes : Int = 0) : View(context, attrs, defStyleAttr, defStyleRes) {
    companion object {
        private val controllerTypeMappings = mapOf(*ControllerType.values().map {
            it to (setOf(*it.buttons) + setOf(*it.optionalButtons) to setOf(*it.sticks))
        }.toTypedArray())
    }

    private var onButtonStateChangedListener : OnButtonStateChangedListener? = null
    fun setOnButtonStateChangedListener(listener : OnButtonStateChangedListener) {
        onButtonStateChangedListener = listener
    }

    private var onStickStateChangedListener : OnStickStateChangedListener? = null
    fun setOnStickStateChangedListener(listener : OnStickStateChangedListener) {
        onStickStateChangedListener = listener
    }

    private val joystickAnimators = mutableMapOf<JoystickButton, Animator?>()
    var controllerType : ControllerType? = null
        set(value) {
            field = value
            invalidate()
        }
    var recenterSticks = false
        set(value) {
            field = value
            controls.joysticks.forEach { it.recenterSticks = recenterSticks }
        }
    var stickRegions = false
        set(value) {
            field = value
            controls.setStickRegions(value)
            invalidate()
        }
    var hapticFeedback = false
        set(value) {
            field = value
            controls.buttons.forEach { it.hapticFeedback = hapticFeedback }
        }

    internal val editInfo = OnScreenEditInfo()
    fun setOnEditButtonChangedListener(listener : OnEditButtonChangedListener?) {
        editInfo.onEditButtonChangedListener = listener
    }

    private val selectionPaint = Paint().apply {
        color = context.obtainStyledAttributes(intArrayOf(R.attr.colorPrimary)).use { it.getColor(0, Color.RED) }
        style = Paint.Style.STROKE
        strokeWidth = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 2f, context.resources.displayMetrics)
    }

    // Populated externally by the activity, as retrieving the vibrator service inside the view crashes the layout editor
    lateinit var vibrator : Vibrator
    private val effectClick = VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK)

    // Ensure controls init happens after editInfo is initialized so that the buttons have a valid reference to it
    private val controls = Controls(this)

    override fun onDraw(canvas : Canvas) {
        super.onDraw(canvas)

        val allowedIds = controllerTypeMappings[controllerType]
        controls.allButtons.forEach { button ->
            if (allowedIds?.let { (buttonIds, stickIds) ->
                    if (button is JoystickButton) stickIds.contains(button.stickId) else buttonIds.contains(button.buttonId)
                } != false
            ) {
                button.width = width
                button.height = height
                button.render(canvas)
            }
        }

        // Draw the selection box around the edit button
        if (editInfo.isEditing && editInfo.editButton is OnScreenButton)
            canvas.drawRect((editInfo.editButton as OnScreenButton).currentBounds, selectionPaint)
    }

    private val playingTouchHandler = OnTouchListener { _, event ->
        var handled = false
        val actionIndex = event.actionIndex
        val pointerId = event.getPointerId(actionIndex)
        val x by lazy { event.getX(actionIndex) }
        val y by lazy { event.getY(actionIndex) }

        controls.buttons.forEach { button ->
            when (event.action and event.actionMasked) {
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    if (pointerId == button.touchPointerId) {
                        button.touchPointerId = -1
                        if (button.onFingerUp(x, y))
                            onButtonStateChangedListener?.invoke(button.buttonId, ButtonState.Released)
                        handled = true
                    } else if (pointerId == button.partnerPointerId) {
                        button.partnerPointerId = -1
                        if (button.onFingerUp(x, y))
                            onButtonStateChangedListener?.invoke(button.buttonId, ButtonState.Released)
                        handled = true
                    }
                }

                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    if (button.config.enabled && button.isTouched(x, y)) {
                        button.touchPointerId = pointerId
                        if (button.onFingerDown(x, y))
                            onButtonStateChangedListener?.invoke(button.buttonId, ButtonState.Pressed)
                        if (hapticFeedback)
                            vibrator.vibrate(effectClick)
                        performClick()
                        handled = true
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    for (fingerId in 0 until event.pointerCount) {
                        if (fingerId == button.touchPointerId) {
                            for (buttonPair in controls.buttonPairs) {
                                if (buttonPair.contains(button)) {
                                    for (otherButton in buttonPair) {
                                        if (otherButton.partnerPointerId == -1 &&
                                            otherButton != button &&
                                            otherButton.config.enabled &&
                                            otherButton.isTouched(event.getX(fingerId), event.getY(fingerId))
                                        ) {
                                            otherButton.partnerPointerId = fingerId
                                            if (otherButton.onFingerDown(x, y))
                                                onButtonStateChangedListener?.invoke(otherButton.buttonId, ButtonState.Pressed)
                                            if (hapticFeedback)
                                                vibrator.vibrate(effectClick)
                                            performClick()
                                            handled = true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (handled) {
            invalidate()
            return@OnTouchListener true
        }

        controls.joysticks.forEach { joystick ->
            when (event.actionMasked) {
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP,
                MotionEvent.ACTION_CANCEL -> {
                    if (pointerId == joystick.touchPointerId) {
                        joystick.touchPointerId = -1

                        val position = PointF(joystick.currentX, joystick.currentY)
                        val radius = joystick.radius
                        val outerToInner = joystick.outerToInner()
                        val outerToInnerLength = outerToInner.length()
                        val direction = outerToInner.normalize()
                        val duration = (50f * outerToInnerLength / radius).roundToLong()
                        joystickAnimators[joystick] = ValueAnimator.ofFloat(outerToInnerLength, 0f).apply {
                            addUpdateListener { animation ->
                                val value = animation.animatedValue as Float
                                val vector = direction.multiply(value)
                                val newPosition = position.add(vector)
                                joystick.onFingerMoved(newPosition.x, newPosition.y, false)
                                onStickStateChangedListener?.invoke(joystick.stickId, vector.multiply(1f / radius))
                                invalidate()
                            }
                            addListener(object : AnimatorListenerAdapter() {
                                override fun onAnimationCancel(animation : Animator) {
                                    super.onAnimationCancel(animation)
                                    onAnimationEnd(animation)
                                    onStickStateChangedListener?.invoke(joystick.stickId, PointF(0f, 0f))
                                }

                                override fun onAnimationEnd(animation : Animator) {
                                    super.onAnimationEnd(animation)
                                    if (joystick.shortDoubleTapped)
                                        onButtonStateChangedListener?.invoke(joystick.buttonId, ButtonState.Released)
                                    joystick.onFingerUp(event.x, event.y)
                                    invalidate()
                                }
                            })
                            setDuration(duration)
                            start()
                        }
                        handled = true
                    }
                }

                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    if (joystick.config.enabled && joystick.isTouched(x, y)) {
                        joystickAnimators[joystick]?.cancel()
                        joystickAnimators[joystick] = null
                        joystick.touchPointerId = pointerId
                        joystick.onFingerDown(x, y)
                        if (joystick.shortDoubleTapped)
                            onButtonStateChangedListener?.invoke(joystick.buttonId, ButtonState.Pressed)
                        if (recenterSticks)
                            onStickStateChangedListener?.invoke(joystick.stickId, joystick.outerToInnerRelative())
                        performClick()
                        handled = true
                    }
                }

                MotionEvent.ACTION_MOVE -> {
                    for (i in 0 until event.pointerCount) {
                        if (event.getPointerId(i) == joystick.touchPointerId) {
                            val centerToPoint = joystick.onFingerMoved(event.getX(i), event.getY(i))
                            onStickStateChangedListener?.invoke(joystick.stickId, centerToPoint)
                            handled = true
                        }
                    }
                }
            }
        }
        handled.also { if (it) invalidate() }
    }

    /**
     * Tracks whether the last pointer down event changed the active edit button
     * Avoids moving the button when the user just wants to select it
     */
    private var activeEditButtonChanged = false

    private val editingTouchHandler = OnTouchListener { _, event ->
        run {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    val touchedButton = controls.allButtons.firstOrNull { it.isTouched(event.x, event.y) } ?: return@OnTouchListener false

                    // Update the selection if the user touched a button other than the selected one
                    if (touchedButton != editInfo.editButton) {
                        activeEditButtonChanged = true
                        editInfo.editButton = touchedButton
                        performClick()
                        invalidate()
                        return@run
                    }

                    editInfo.editButton.startMove(event.x, event.y)
                }

                MotionEvent.ACTION_MOVE -> {
                    // If the user just selected another button, don't move it yet
                    if (activeEditButtonChanged)
                        return@run

                    editInfo.editButton.move(event.x, event.y)
                    invalidate()
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP,
                MotionEvent.ACTION_CANCEL -> {
                    if (activeEditButtonChanged) {
                        activeEditButtonChanged = false
                        return@run
                    }

                    editInfo.editButton.endMove()
                }
            }
        }
        true
    }

    init {
        setOnTouchListener(playingTouchHandler)
    }

    fun updateEditButtonInfo() {
        editInfo.onEditButtonChangedListener?.invoke(editInfo.editButton)
    }

    fun setEditMode(isEdit : Boolean) {
        // Select all buttons when entering edit if we weren't already editing
        if (!editInfo.isEditing)
            selectAllButtons()
        editInfo.isEditing = isEdit
        setOnTouchListener(if (isEdit) editingTouchHandler else playingTouchHandler)
        invalidate()
    }

    fun selectAllButtons() {
        editInfo.editButton = allButtonsProxy
        invalidate()
    }

    fun setButtonEnabled(enabled : Boolean) {
        editInfo.editButton.config.enabled = enabled
        invalidate()
    }

    fun setButtonToggleMode(toggleMode : Boolean) {
        editInfo.editButton.config.toggleMode = toggleMode
        invalidate()
    }

    fun setButtonScale(@IntRange(from = 0, to = 100) scale : Int) {
        fun toScaleRange(value : Int) : Float = (value / 100f) * (OnScreenConfiguration.MaxScale - OnScreenConfiguration.MinScale) + OnScreenConfiguration.MinScale

        editInfo.editButton.config.scale = toScaleRange(scale)
        invalidate()
    }

    fun setButtonOpacity(@IntRange(from = 0, to = 100) opacity : Int) {
        fun toAlphaRange(value : Int) : Int = ((value / 100f) * (OnScreenConfiguration.MaxAlpha - OnScreenConfiguration.MinAlpha)).toInt() + OnScreenConfiguration.MinAlpha

        editInfo.editButton.config.alpha = toAlphaRange(opacity)
        invalidate()
    }

    fun setStickActivationRadius(@IntRange(from = 0, to = 100) radius : Int) {
        fun toRadiusRange(value : Int) : Float = (value / 100f) * (OnScreenConfiguration.MaxActivationRadius - OnScreenConfiguration.MinActivationRadius) + OnScreenConfiguration.MinActivationRadius

        (editInfo.editButton as? JoystickButton)?.let { joystick ->
            joystick.config.activationRadius = toRadiusRange(radius)
            joystick.loadActivationRadius()
        }
        invalidate()
    }

    fun moveButtonUp() {
        editInfo.editButton.moveUp()
        invalidate()
    }

    fun moveButtonDown() {
        editInfo.editButton.moveDown()
        invalidate()
    }

    fun moveButtonLeft() {
        editInfo.editButton.moveLeft()
        invalidate()
    }

    fun moveButtonRight() {
        editInfo.editButton.moveRight()
        invalidate()
    }

    // Used to retrieve the current color to use in the color picker dialog
    fun getButtonTextColor() = editInfo.editButton.config.textColor
    fun getButtonBackgroundColor() = editInfo.editButton.config.backgroundColor

    fun setButtonTextColor(color : Int) {
        editInfo.editButton.config.textColor = color
        invalidate()
    }

    fun setButtonBackgroundColor(color : Int) {
        editInfo.editButton.config.backgroundColor = color
        invalidate()
    }

    fun setSnapToGrid(snap : Boolean) {
        editInfo.snapToGrid = snap
    }

    fun resetButton() {
        editInfo.editButton.resetConfig()
        editInfo.onEditButtonChangedListener?.invoke(editInfo.editButton)
        invalidate()
    }

    /**
     * A proxy button that is used to apply changes to all buttons
     */
    private val allButtonsProxy = object : ConfigurableButton {
        override val buttonId : ButtonId = ButtonId.All

        override val config = object : OnScreenConfiguration {
            override var enabled : Boolean
                get() = controls.allButtons.all { it.config.enabled }
                set(value) {
                    controls.allButtons.forEach { it.config.enabled = value }
                }

            override val groupEnabled : Int
                get() {
                    if (controls.allButtons.all { it.config.enabled })
                        return OnScreenConfiguration.GroupEnabled
                    if (controls.allButtons.all { !it.config.enabled })
                        return OnScreenConfiguration.GroupDisabled
                    return OnScreenConfiguration.GroupIndeterminate
                }

            override var toggleMode : Boolean
                get() = controls.allButtons.all { it.supportsToggleMode() == it.config.toggleMode }
                set(value) {
                    controls.allButtons.forEach { if (it.supportsToggleMode()) it.config.toggleMode = value }
                }

            override val groupToggleMode : Int
                get() {
                    if (controls.allButtons.all { !it.supportsToggleMode() || it.config.toggleMode })
                        return OnScreenConfiguration.GroupEnabled
                    if (controls.allButtons.all { !it.supportsToggleMode() || !it.config.toggleMode })
                        return OnScreenConfiguration.GroupDisabled
                    return OnScreenConfiguration.GroupIndeterminate
                }

            override var alpha : Int
                get() = controls.allButtons.sumOf { it.config.alpha } / controls.allButtons.size
                set(value) {
                    controls.allButtons.forEach { it.config.alpha = value }
                }

            override var textColor : Int
                get() = controls.allButtons.first().config.textColor
                set(value) {
                    controls.allButtons.forEach { it.config.textColor = value }
                }

            override var backgroundColor : Int
                get() = controls.allButtons.first().config.backgroundColor
                set(value) {
                    controls.allButtons.forEach { it.config.backgroundColor = value }
                }

            override var scale : Float
                get() = (controls.allButtons.sumOf { it.config.scale.toDouble() } / controls.allButtons.size).toFloat()
                set(value) {
                    controls.allButtons.forEach { it.config.scale = value }
                }

            override var relativeX = 0f
            override var relativeY = 0f
            override var activationRadius = 0f
        }

        override fun startMove(x : Float, y : Float) {}
        override fun move(x : Float, y : Float) {}
        override fun endMove() {}

        override fun moveUp() {
            controls.allButtons.forEach { it.moveUp() }
        }

        override fun moveDown() {
            controls.allButtons.forEach { it.moveDown() }
        }

        override fun moveLeft() {
            controls.allButtons.forEach { it.moveLeft() }
        }

        override fun moveRight() {
            controls.allButtons.forEach { it.moveRight() }
        }

        override fun resetConfig() {
            controls.allButtons.forEach { it.resetConfig() }
        }
    }
}
