/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.view.KeyEvent
import android.view.MotionEvent
import java.io.Serializable

/**
 * This a sealed class for all host events that is inherited by all other event classes
 *
 * @param descriptor The device descriptor of the device this event originates from
 */
sealed class HostEvent(open val descriptor : String = "") : Serializable {
    /**
     * The [toString] function is abstract so that the derived classes can return a proper string
     */
    abstract override fun toString() : String
}

/**
 * This class represents all events on the host that arise from a [KeyEvent]
 */
data class KeyHostEvent(override val descriptor : String = "", val keyCode : Int) : HostEvent(descriptor) {
    /**
     * This returns the string representation of [keyCode]
     */
    override fun toString() : String = KeyEvent.keyCodeToString(keyCode)
}

/**
 * This class represents all events on the host that arise from a [MotionEvent]
 */
data class MotionHostEvent(override val descriptor : String = "", val axis : Int, val polarity : Boolean) : HostEvent(descriptor) {
    companion object {
        /**
         * This is an array of all the axes that are checked during a [MotionEvent]
         */
        val axes = arrayOf(
                MotionEvent.AXIS_X,
                MotionEvent.AXIS_Y,
                MotionEvent.AXIS_Z,
                MotionEvent.AXIS_RZ,
                MotionEvent.AXIS_HAT_X,
                MotionEvent.AXIS_HAT_Y,
                MotionEvent.AXIS_LTRIGGER,
                MotionEvent.AXIS_RTRIGGER,
                MotionEvent.AXIS_THROTTLE,
                MotionEvent.AXIS_RUDDER,
                MotionEvent.AXIS_WHEEL,
                MotionEvent.AXIS_GAS,
                MotionEvent.AXIS_BRAKE) + (IntRange(MotionEvent.AXIS_GENERIC_1, MotionEvent.AXIS_GENERIC_16).toList())
    }

    /**
     * This returns the string representation of [axis] combined with [polarity]
     */
    override fun toString() : String = MotionEvent.axisToString(axis) + if (polarity) "+" else "-"
}
