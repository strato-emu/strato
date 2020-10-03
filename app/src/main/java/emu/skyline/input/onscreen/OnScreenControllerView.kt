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
import android.graphics.PointF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import android.view.View.OnTouchListener
import emu.skyline.input.ButtonId
import emu.skyline.input.ButtonState
import kotlin.math.roundToLong

typealias OnButtonStateChangedListener = (buttonId : ButtonId, state : ButtonState) -> Unit
typealias OnStickStateChangedListener = (buttonId : ButtonId, position : PointF) -> Unit

class OnScreenControllerView @JvmOverloads constructor(
        context : Context,
        attrs : AttributeSet? = null,
        defStyleAttr : Int = 0,
        defStyleRes : Int = 0
) : View(context, attrs, defStyleAttr, defStyleRes) {
    private val controls = Controls(this)
    private var onButtonStateChangedListener : OnButtonStateChangedListener? = null
    private var onStickStateChangedListener : OnStickStateChangedListener? = null
    private val joystickAnimators = mutableMapOf<JoystickButton, Animator?>()

    override fun onDraw(canvas : Canvas) {
        super.onDraw(canvas)

        (controls.circularButtons + controls.rectangularButtons + controls.triggerButtons + controls.joysticks).forEach {
            it.width = width
            it.height = height
            it.render(canvas)
        }
    }

    private val playingTouchHandler = OnTouchListener { _, event ->
        var handled = false
        val actionIndex = event.actionIndex
        val pointerId = event.getPointerId(actionIndex)
        val x by lazy { event.getX(actionIndex) }
        val y by lazy { event.getY(actionIndex) }

        (controls.circularButtons + controls.rectangularButtons + controls.triggerButtons).forEach { button ->
            when (event.action and event.actionMasked) {
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> {
                    if (pointerId == button.touchPointerId) {
                        button.touchPointerId = -1
                        button.onFingerUp(x, y)
                        onButtonStateChangedListener?.invoke(button.buttonId, ButtonState.Released)
                        handled = true
                    }
                }
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    if (button.isTouched(x, y)) {
                        button.touchPointerId = pointerId
                        button.onFingerDown(x, y)
                        performClick()
                        onButtonStateChangedListener?.invoke(button.buttonId, ButtonState.Pressed)
                        handled = true
                    }
                }
            }
        }

        for (joystick in controls.joysticks) {
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
                                joystick.onFingerMoved(newPosition.x, newPosition.y)
                                onStickStateChangedListener?.invoke(joystick.buttonId, vector.multiply(1f / radius))
                                invalidate()
                            }
                            addListener(object : AnimatorListenerAdapter() {
                                override fun onAnimationCancel(animation : Animator?) {
                                    super.onAnimationCancel(animation)
                                    onAnimationEnd(animation)
                                    onStickStateChangedListener?.invoke(joystick.buttonId, PointF(0f, 0f))
                                }

                                override fun onAnimationEnd(animation : Animator?) {
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
                    if (joystick.isTouched(x, y)) {
                        joystickAnimators[joystick]?.cancel()
                        joystickAnimators[joystick] = null
                        joystick.touchPointerId = pointerId
                        joystick.onFingerDown(x, y)
                        if (joystick.shortDoubleTapped)
                            onButtonStateChangedListener?.invoke(joystick.buttonId, ButtonState.Pressed)
                        performClick()
                        handled = true
                    }
                }
                MotionEvent.ACTION_MOVE -> {
                    for (i in 0 until event.pointerCount) {
                        if (event.getPointerId(i) == joystick.touchPointerId) {
                            val centerToPoint = joystick.onFingerMoved(event.getX(i), event.getY(i))
                            onStickStateChangedListener?.invoke(joystick.buttonId, centerToPoint)
                            handled = true
                        }
                    }
                }
            }
        }

        handled.also { if (it) invalidate() else super.onTouchEvent(event) }
    }

    private val editingTouchHandler = OnTouchListener { _, event ->
        (controls.circularButtons + controls.rectangularButtons + controls.triggerButtons + controls.joysticks).any { button ->
            when (event.actionMasked) {
                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP,
                MotionEvent.ACTION_CANCEL -> {
                    if (button.isEditing) {
                        button.endEdit()
                        return@any true
                    }
                }
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> {
                    if (button.isTouched(event.x, event.y)) {
                        button.startEdit()
                        performClick()
                        return@any true
                    }
                }
                MotionEvent.ACTION_MOVE -> {
                    if (button.isEditing) {
                        button.edit(event.x, event.y)
                        return@any true
                    }
                }
            }
            false
        }.also { handled -> if (handled) invalidate() else super.onTouchEvent(event) }
    }

    init {
        setOnTouchListener(playingTouchHandler)
    }

    fun setEditMode(editMode : Boolean) = setOnTouchListener(if (editMode) editingTouchHandler else playingTouchHandler)

    fun resetControls() {
        (controls.circularButtons + controls.rectangularButtons + controls.triggerButtons + controls.joysticks).forEach { it.resetRelativeValues() }
        controls.globalScale = 1f
        invalidate()
    }

    fun increaseScale() {
        controls.globalScale *= 1.1f
        invalidate()
    }

    fun decreaseScale() {
        controls.globalScale *= 0.9f
        invalidate()
    }

    fun setOnButtonStateChangedListener(listener : OnButtonStateChangedListener) {
        onButtonStateChangedListener = listener
    }

    fun setOnStickStateChangedListener(listener : OnStickStateChangedListener) {
        onStickStateChangedListener = listener
    }
}