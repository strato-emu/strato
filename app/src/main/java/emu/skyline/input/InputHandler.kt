/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import emu.skyline.utils.PreferenceSettings
import kotlin.math.abs

/**
 * Handles input events during emulation
 */
class InputHandler(private val inputManager : InputManager, private val preferenceSettings : PreferenceSettings) {
    companion object {
        /**
         * This initializes a guest controller in libskyline
         *
         * @param index The arbitrary index of the controller, this is to handle matching with a partner Joy-Con
         * @param type The type of the host controller
         * @param partnerIndex The index of a partner Joy-Con if there is one
         * @note This is blocking and will stall till input has been initialized on the guest
         */
        external fun setController(index : Int, type : Int, partnerIndex : Int = -1)

        /**
         * This flushes the controller updates on the guest
         *
         * @note This is blocking and will stall till input has been initialized on the guest
         */
        external fun updateControllers()

        /**
         * This sets the state of the buttons specified in the mask on a specific controller
         *
         * @param index The index of the controller this is directed to
         * @param mask The mask of the button that are being set
         * @param pressed If the buttons are being pressed or released
         */
        external fun setButtonState(index : Int, mask : Long, pressed : Boolean)

        /**
         * This sets the value of a specific axis on a specific controller
         *
         * @param index The index of the controller this is directed to
         * @param axis The ID of the axis that is being modified
         * @param value The value to set the axis to
         */
        external fun setAxisValue(index : Int, axis : Int, value : Int)

        /**
         * This sets the values of the points on the guest touch-screen
         *
         * @param points An array of skyline::input::TouchScreenPoint in C++ represented as integers
         */
        external fun setTouchState(points : IntArray)
    }

    /**
     * Initializes all of the controllers from [InputManager] on the guest
     */
    fun initializeControllers() {
        for (controller in inputManager.controllers.values) {
            if (controller.type != ControllerType.None) {
                val type = when (controller.type) {
                    ControllerType.None -> throw IllegalArgumentException()
                    ControllerType.HandheldProController -> if (preferenceSettings.isDocked) ControllerType.ProController.id else ControllerType.HandheldProController.id
                    ControllerType.ProController, ControllerType.JoyConLeft, ControllerType.JoyConRight -> controller.type.id
                }

                val partnerIndex = when (controller) {
                    is JoyConLeftController -> controller.partnerId
                    is JoyConRightController -> controller.partnerId
                    else -> null
                }

                setController(controller.id, type, partnerIndex ?: -1)
            }
        }

        updateControllers()
    }

    /**
     * Handles translating any [KeyHostEvent]s to a [GuestEvent] that is passed into libskyline
     */
    fun handleKeyEvent(event : KeyEvent) : Boolean {
        if (event.repeatCount != 0)
            return false

        val action = when (event.action) {
            KeyEvent.ACTION_DOWN -> ButtonState.Pressed
            KeyEvent.ACTION_UP -> ButtonState.Released
            else -> return false
        }

        return when (val guestEvent = inputManager.eventMap[KeyHostEvent(event.device.descriptor, event.keyCode)]) {
            is ButtonGuestEvent -> {
                if (guestEvent.button != ButtonId.Menu)
                    setButtonState(guestEvent.id, guestEvent.button.value(), action.state)
                true
            }

            is AxisGuestEvent -> {
                setAxisValue(guestEvent.id, guestEvent.axis.ordinal, (if (action == ButtonState.Pressed) if (guestEvent.polarity) Short.MAX_VALUE else Short.MIN_VALUE else 0).toInt())
                true
            }

            else -> false
        }
    }

    /**
     * The last value of the axes so the stagnant axes can be eliminated to not wastefully look them up
     */
    private val axesHistory = FloatArray(MotionHostEvent.axes.size)

    /**
     * Handles translating any [MotionHostEvent]s to a [GuestEvent] that is passed into libskyline
     */
    fun handleMotionEvent(event : MotionEvent) : Boolean {
        if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE) {
            for (axisItem in MotionHostEvent.axes.withIndex()) {
                val axis = axisItem.value
                var value = event.getAxisValue(axis)

                if ((event.historySize != 0 && value != event.getHistoricalAxisValue(axis, 0)) || axesHistory[axisItem.index] != value) {
                    var polarity = value > 0 || (value == 0f && axesHistory[axisItem.index] >= 0)

                    val guestEvent = MotionHostEvent(event.device.descriptor, axis, polarity).let { hostEvent ->
                        inputManager.eventMap[hostEvent] ?: if (value == 0f) {
                            polarity = false
                            inputManager.eventMap[hostEvent.copy(polarity = false)]
                        } else {
                            null
                        }
                    }

                    when (guestEvent) {
                        is ButtonGuestEvent -> {
                            val action = if (abs(value) >= guestEvent.threshold) ButtonState.Pressed.state else ButtonState.Released.state
                            if (guestEvent.button != ButtonId.Menu)
                                setButtonState(guestEvent.id, guestEvent.button.value(), action)
                        }

                        is AxisGuestEvent -> {
                            value = guestEvent.value(value)
                            value = if (polarity) abs(value) else -abs(value)
                            value = if (guestEvent.axis == AxisId.LX || guestEvent.axis == AxisId.RX) value else -value
                            setAxisValue(guestEvent.id, guestEvent.axis.ordinal, (value * Short.MAX_VALUE).toInt())
                        }
                    }
                }

                axesHistory[axisItem.index] = value
            }

            return true
        }

        return false
    }

    fun handleTouchEvent(view : View, event : MotionEvent) : Boolean {
        val count = event.pointerCount
        val points = IntArray(count * 7) // This is an array of skyline::input::TouchScreenPoint in C++ as that allows for efficient transfer of values to it
        var offset = 0
        for (index in 0 until count) {
            val pointer = MotionEvent.PointerCoords()
            event.getPointerCoords(index, pointer)

            val x = 0f.coerceAtLeast(pointer.x * 1280 / view.width).toInt()
            val y = 0f.coerceAtLeast(pointer.y * 720 / view.height).toInt()

            val attribute = when (event.action) {
                MotionEvent.ACTION_DOWN -> 1
                MotionEvent.ACTION_UP -> 2
                else -> 0
            }

            points[offset++] = attribute
            points[offset++] = event.getPointerId(index)
            points[offset++] = x
            points[offset++] = y
            points[offset++] = pointer.touchMinor.toInt()
            points[offset++] = pointer.touchMajor.toInt()
            points[offset++] = (pointer.orientation * 180 / Math.PI).toInt()
        }

        setTouchState(points)

        return true
    }

    fun getFirstControllerType() : ControllerType {
        return inputManager.controllers[0]?.type ?: ControllerType.None
    }
}
