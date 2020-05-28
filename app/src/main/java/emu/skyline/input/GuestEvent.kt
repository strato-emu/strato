/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import java.io.Serializable
import java.util.*
import kotlin.math.abs

/**
 * This enumerates all of the buttons that the emulator recognizes
 */
enum class ButtonId(val short : String? = null, val long : Int? = null) {
    A("A", emu.skyline.R.string.a_button),
    B("B", emu.skyline.R.string.b_button),
    X("X", emu.skyline.R.string.x_button),
    Y("Y", emu.skyline.R.string.y_button),
    LeftStick("L"),
    RightStick("R"),
    L("L", emu.skyline.R.string.left_shoulder),
    R("R", emu.skyline.R.string.right_shoulder),
    ZL("ZL", emu.skyline.R.string.left_trigger),
    ZR("ZR", emu.skyline.R.string.right_trigger),
    Plus("+", emu.skyline.R.string.plus_button),
    Minus("-", emu.skyline.R.string.minus_button),
    DpadLeft("◀", emu.skyline.R.string.left),
    DpadUp("▲", emu.skyline.R.string.up),
    DpadRight("▶", emu.skyline.R.string.right),
    DpadDown("▼", emu.skyline.R.string.down),
    LeftStickLeft,
    LeftStickUp,
    LeftStickRight,
    LeftStickDown,
    RightStickLeft,
    RightStickUp,
    RightStickRight,
    RightStickDown,
    LeftSL("SL", emu.skyline.R.string.left_shoulder),
    LeftSR("SR", emu.skyline.R.string.right_shoulder),
    RightSL("SL", emu.skyline.R.string.left_shoulder),
    RightSR("SR", emu.skyline.R.string.right_shoulder),
    Menu("⌂", emu.skyline.R.string.emu_menu_button);

    /**
     * This returns the value as setting the [ordinal]-th bit in a [Long]
     */
    fun value() : Long {
        return (1.toLong()) shl ordinal
    }
}

/**
 * This enumerates the states of a button and denotes their Boolean values in [state]
 */
enum class ButtonState(val state : Boolean) {
    Released(false),
    Pressed(true),
}

/**
 * This enumerates all of the axes on a controller that the emulator recognizes
 */
enum class AxisId {
    RX,
    RY,
    LX,
    LY,
}

/**
 * This enumerates all the sticks on a controller with all their components
 *
 * @param xAxis The [AxisId] corresponding to movement on the X-axis for the stick
 * @param yAxis The [AxisId] corresponding to movement on the Y-axis for the stick
 * @param button The [ButtonId] of the button activated when the stick is pressed
 */
enum class StickId(val xAxis : AxisId, val yAxis : AxisId, val button : ButtonId) {
    Left(AxisId.LX, AxisId.LY, ButtonId.LeftStick),
    Right(AxisId.RX, AxisId.RY, ButtonId.RightStick);

    override fun toString() = "$name Stick"
}

/**
 * This an abstract class for all guest events that is inherited by all other event classes
 *
 * @param id The ID of the guest controller this event corresponds to
 */
abstract class GuestEvent(val id : Int) : Serializable {
    /**
     * The equality function is abstract so that equality checking will be for the derived classes rather than this abstract class
     */
    abstract override fun equals(other : Any?) : Boolean

    /**
     * The hash function is abstract so that hashes will be generated for the derived classes rather than this abstract class
     */
    abstract override fun hashCode() : Int
}

/**
 * This class is used for all guest events that correspond to a button
 *
 * @param button The ID of the button that this represents
 * @param threshold The threshold of a corresponding [MotionHostEvent]'s axis value for this to be "pressed"
 */
class ButtonGuestEvent(id : Int, val button : ButtonId, val threshold : Float = 0f) : GuestEvent(id) {
    /**
     * This does some basic equality checking for the type of [other] and all members in the class except [threshold] as that is irrelevant for a lookup
     */
    override fun equals(other : Any?) : Boolean = if (other is ButtonGuestEvent) this.id == other.id && this.button == other.button else false

    /**
     * This computes the hash for all members of the class except [threshold] as that is irrelevant for a lookup
     */
    override fun hashCode() : Int = Objects.hash(id, button)
}

/**
 * This class is used for all guest events that correspond to a specific pole of an axis
 *
 * @param axis The ID of the axis that this represents
 * @param polarity The polarity of the axis this represents
 * @param max The maximum recorded value of the corresponding [MotionHostEvent] to scale the axis value
 */
class AxisGuestEvent(id : Int, val axis : AxisId, val polarity : Boolean, var max : Float = 1f) : GuestEvent(id) {
    /**
     * This does some basic equality checking for the type of [other] and all members in the class except [max] as that is irrelevant for a lookup
     */
    override fun equals(other : Any?) : Boolean = if (other is AxisGuestEvent) this.id == other.id && this.axis == other.axis && this.polarity == other.polarity else false

    /**
     * This computes the hash for all members of the class except [max] as that is irrelevant for a lookup
     */
    override fun hashCode() : Int = Objects.hash(id, axis, polarity)

    /**
     * This is used to retrieve the scaled value/update the maximum value of this axis
     *
     * @param axis The unscaled value of the axis to scale
     * @return The scaled value of this axis
     */
    fun value(axis : Float) : Float {
        if (max == 1f) return axis

        val axisAbs = abs(axis)
        if (axisAbs >= max) {
            max = axisAbs
            return 1f
        }

        return axis + (axis * (1f - max))
    }
}
