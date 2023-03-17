/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.view.*
import androidx.core.content.getSystemService
import emu.skyline.settings.EmulationSettings
import emu.skyline.utils.ByteBufferSerializable
import emu.skyline.utils.u64
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.abs

/**
 * Handles input events during emulation
 */
class InputHandler(private val inputManager : InputManager, private val emulationSettings : EmulationSettings) : SensorEventListener {
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
         * This sets the values of the motion sensor on a specific controller
         *
         * @param index The index of the controller this is directed to
         * @param motionId The ID of the motion sensor that is being modified
         * @param value A byte buffer of skyline::input::MotionInput in C++
         */
        private external fun setMotionState(index : Int, motionId : Int, value : ByteBuffer)

        /**
         * This sets the values of the points on the guest touch-screen
         *
         * @param points An array of skyline::input::TouchScreenPoint in C++ represented as integers
         */
        external fun setTouchState(points : IntArray)
    }

    @Suppress("ArrayInDataClass")
    data class MotionSensorInput(
        var timestamp : u64 = 0uL,
        var deltaTimestamp : u64 = 0uL,
        @param:ByteBufferSerializable.ByteBufferSerializableArray(3) var gyroscope : FloatArray = FloatArray(3),
        @param:ByteBufferSerializable.ByteBufferSerializableArray(3) var accelerometer : FloatArray = FloatArray(3),
        @param:ByteBufferSerializable.ByteBufferSerializableArray(4) var quaternion : FloatArray = FloatArray(4),
        @param:ByteBufferSerializable.ByteBufferSerializableArray(9) var orientationMatrix : FloatArray = FloatArray(9),
    ) : ByteBufferSerializable

    /**
     * The latest state of the motion sensor
     */
    private val motionSensor = MotionSensorInput()

    /**
     * Buffer for passing motion data to c++
     */
    private val motionDataBufferSize = 0x5C
    private val motionDataBuffer = ByteBuffer.allocateDirect(motionDataBufferSize).order(ByteOrder.LITTLE_ENDIAN)

    /**
     * Used for adjusting motion to phone orientation
     */
    private val motionRotationMatrix = FloatArray(9)
    private val motionGyroOrientation : FloatArray = FloatArray(3)
    private val motionAcelOrientation : FloatArray = FloatArray(3)
    private var motionAxisOrientationX = SensorManager.AXIS_Y
    private var motionAxisOrientationY = SensorManager.AXIS_X

    /**
     * Initializes all of the controllers from [InputManager] on the guest
     */
    fun initializeControllers() {
        for (controller in inputManager.controllers.values) {
            if (controller.type != ControllerType.None) {
                val type = when (controller.type) {
                    ControllerType.None -> throw IllegalArgumentException()
                    ControllerType.HandheldProController -> if (emulationSettings.isDocked) ControllerType.ProController.id else ControllerType.HandheldProController.id
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

    fun initialiseMotionSensors(context : Context) {
        val sensorManager = context.getSystemService<SensorManager>() ?: return
        val sensorList = sensorManager.getSensorList(Sensor.TYPE_ALL)
        val hasRotationVector = sensorList.any { sensor -> sensor.type == Sensor.TYPE_ROTATION_VECTOR }

        sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)?.also { accelerometer ->
            sensorManager.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME)
        }
        sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)?.also { gyroscope ->
            sensorManager.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME)
        }
        sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)?.also { rotationVector ->
            sensorManager.registerListener(this, rotationVector, SensorManager.SENSOR_DELAY_GAME)
        }
        // Avoid listening to two rotation vectors at once
        if (!hasRotationVector) {
            sensorManager.getDefaultSensor(Sensor.TYPE_GAME_ROTATION_VECTOR)?.also { rotationVector ->
                sensorManager.registerListener(this, rotationVector, SensorManager.SENSOR_DELAY_GAME)
            }
        }

        setMotionOrientation90()
        val orientationEventListener = object : OrientationEventListener(context) {
            override fun onOrientationChanged(orientation : Int) {
                when {
                    isWithinOrientationRange(orientation, 270) -> {
                        setMotionOrientation270()
                    }
                    isWithinOrientationRange(orientation, 90) -> {
                        setMotionOrientation90()
                    }
                }
            }

            private fun isWithinOrientationRange(
                currentOrientation : Int, targetOrientation : Int, epsilon : Int = 90
            ) : Boolean {
                return currentOrientation > targetOrientation - epsilon
                        && currentOrientation < targetOrientation + epsilon
            }
        }
        orientationEventListener.enable()
    }

    /**
     * Configures motion axis to a 90° angle
     */
    fun setMotionOrientation90() {
        motionGyroOrientation[0] = 1.0f
        motionGyroOrientation[1] = -1.0f
        motionGyroOrientation[2] = 1.0f
        motionAcelOrientation[0] = -1.0f
        motionAcelOrientation[1] = 1.0f
        motionAcelOrientation[2] = -1.0f
        motionAxisOrientationX = SensorManager.AXIS_Y
        motionAxisOrientationY = SensorManager.AXIS_X
    }

    /**
     * Configures motion axis to a 270° angle
     */
    fun setMotionOrientation270() {
        motionGyroOrientation[0] = -1.0f
        motionGyroOrientation[1] = 1.0f
        motionGyroOrientation[2] = 1.0f
        motionAcelOrientation[0] = 1.0f
        motionAcelOrientation[1] = -1.0f
        motionAcelOrientation[2] = -1.0f

        // TODO: Find the correct configuration here
        motionAxisOrientationX = SensorManager.AXIS_Y
        motionAxisOrientationY = SensorManager.AXIS_X
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
                    setButtonState(guestEvent.id, guestEvent.button.value, action.state)
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
                val range : InputDevice.MotionRange? = event.device.getMotionRange(axis, event.source)
                var value = event.getAxisValue(axis)
                range?.let {
                    value = if (abs(value) > it.flat)
                        if (value > 0)
                            (value - it.flat) / (it.max - it.flat)
                        else
                            -((abs(value) - it.flat) / (abs(it.min) - it.flat))
                    else
                        0f
                }

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
                                setButtonState(guestEvent.id, guestEvent.button.value, action)
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

    override fun onAccuracyChanged(sensor : Sensor?, accuracy : Int) {}

    /**
     * This handles translating any [SensorEvent]s to a [GuestEvent] that is passed into libskyline
     */
    override fun onSensorChanged(event : SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_ACCELEROMETER -> {
                motionSensor.accelerometer[0] = motionAcelOrientation[0] * event.values[1] / SensorManager.GRAVITY_EARTH
                motionSensor.accelerometer[1] = motionAcelOrientation[1] * event.values[0] / SensorManager.GRAVITY_EARTH
                motionSensor.accelerometer[2] = motionAcelOrientation[2] * event.values[2] / SensorManager.GRAVITY_EARTH
            }

            Sensor.TYPE_GYROSCOPE -> {
                // Investigate why sensor value is off by 12x
                motionSensor.gyroscope[0] = motionGyroOrientation[0] * event.values[1] / 12.0f
                motionSensor.gyroscope[1] = motionGyroOrientation[1] * event.values[0] / 12.0f
                motionSensor.gyroscope[2] = motionGyroOrientation[2] * event.values[2] / 12.0f
            }

            Sensor.TYPE_ROTATION_VECTOR -> {
                motionSensor.quaternion[0] = event.values[1]
                motionSensor.quaternion[1] = event.values[0]
                motionSensor.quaternion[2] = event.values[2]
                motionSensor.quaternion[3] = event.values[3]
                SensorManager.getRotationMatrixFromVector(motionRotationMatrix, motionSensor.quaternion)
                SensorManager.remapCoordinateSystem(motionRotationMatrix, motionAxisOrientationX, motionAxisOrientationY, motionSensor.orientationMatrix)
            }

            Sensor.TYPE_GAME_ROTATION_VECTOR -> {
                motionSensor.quaternion[0] = event.values[1]
                motionSensor.quaternion[1] = event.values[0]
                motionSensor.quaternion[2] = event.values[2]
                motionSensor.quaternion[3] = event.values[3]
                SensorManager.getRotationMatrixFromVector(motionRotationMatrix, motionSensor.quaternion)
                SensorManager.remapCoordinateSystem(motionRotationMatrix, motionAxisOrientationX, motionAxisOrientationY, motionSensor.orientationMatrix)
            }

            else -> {}
        }

        // Only update state on accelerometer data
        if (event.sensor.type != Sensor.TYPE_ACCELEROMETER)
            return

        motionSensor.deltaTimestamp = event.timestamp.toULong() - motionSensor.timestamp
        motionSensor.timestamp = event.timestamp.toULong()
        motionDataBuffer.clear()
        setMotionState(0, 0, motionSensor.writeToByteBuffer(motionDataBuffer))
        motionDataBuffer.clear()
        setMotionState(0, 1, motionSensor.writeToByteBuffer(motionDataBuffer))
        motionDataBuffer.clear()
        setMotionState(0, 2, motionSensor.writeToByteBuffer(motionDataBuffer))
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
