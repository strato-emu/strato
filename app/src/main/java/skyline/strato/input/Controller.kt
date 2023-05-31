/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.input

import skyline.strato.R
import java.io.Serializable

/**
 * This enumerates the types of Controller that can be emulated
 *
 * @param stringRes The string resource of the controller's name
 * @param firstController If the type only applies to the first controller
 * @param optionalButtons Optional buttons that are allowed to be displayed on screen, but shouldn't be included in the controller mapping activity
 */
enum class ControllerType(val stringRes : Int, val firstController : Boolean, val sticks : Array<StickId> = arrayOf(), val buttons : Array<ButtonId> = arrayOf(), val optionalButtons : Array<ButtonId> = arrayOf(), val id : Int) {
    None(R.string.none, false, id = 0b0),
    ProController(
        R.string.procon,
        false,
        arrayOf(StickId.Left, StickId.Right),
        arrayOf(
            ButtonId.A, ButtonId.B, ButtonId.X, ButtonId.Y,
            ButtonId.DpadUp, ButtonId.DpadDown, ButtonId.DpadLeft, ButtonId.DpadRight,
            ButtonId.L, ButtonId.R, ButtonId.ZL, ButtonId.ZR, ButtonId.Plus, ButtonId.Minus
        ),
        arrayOf(ButtonId.L3, ButtonId.R3),
        0b1
    ),
    HandheldProController(
        R.string.handheld_procon,
        true,
        arrayOf(StickId.Left, StickId.Right),
        arrayOf(
            ButtonId.A, ButtonId.B, ButtonId.X, ButtonId.Y,
            ButtonId.DpadUp, ButtonId.DpadDown, ButtonId.DpadLeft, ButtonId.DpadRight,
            ButtonId.L, ButtonId.R, ButtonId.ZL, ButtonId.ZR, ButtonId.Plus, ButtonId.Minus
        ),
        arrayOf(ButtonId.L3, ButtonId.R3),
        0b10
    ),
    JoyConLeft(
        R.string.ljoycon,
        false,
        arrayOf(StickId.Left),
        arrayOf(
            ButtonId.DpadUp, ButtonId.DpadDown, ButtonId.DpadLeft, ButtonId.DpadRight,
            ButtonId.L, ButtonId.ZL, ButtonId.Minus, ButtonId.LeftSL, ButtonId.LeftSR
        ),
        arrayOf(ButtonId.L3),
        0b1000
    ),
    JoyConRight(
        R.string.rjoycon,
        false,
        arrayOf(StickId.Right),
        arrayOf(
            ButtonId.A, ButtonId.B, ButtonId.X, ButtonId.Y,
            ButtonId.R, ButtonId.ZR, ButtonId.Plus, ButtonId.RightSL, ButtonId.RightSR
        ),
        arrayOf(ButtonId.R3),
        0b10000
    ),
}

/**
 * The enumerates the type of general settings for a Controller
 *
 * @param stringRes The string resource for the setting
 * @param compatibleControllers An array of the types of compatible controllers
 */
enum class GeneralType(val stringRes : Int, val compatibleControllers : Array<ControllerType>? = null) {
    PartnerJoyCon(R.string.partner_joycon, arrayOf(ControllerType.JoyConLeft)),
    RumbleDevice(R.string.rumble_device),
    SetupGuide(R.string.setup_guide),
}

/**
 * This is the base class for all controllers, when controllers require to store more variables it'll be stored here
 *
 * @param id The ID of the controller
 * @param type The type of the controller
 * @param rumbleDeviceDescriptor The device descriptor of the device rumble/force-feedback will be passed onto
 * @param rumbleDeviceName The name of the device rumble/force-feedback will be passed onto
 */
open class Controller(val id : Int, var type : ControllerType, var rumbleDeviceDescriptor : String? = null, var rumbleDeviceName : String? = null) : Serializable {
    /**
     * The current version of this class so that different versions won't be deserialized mistakenly
     */
    companion object {
        @JvmStatic
        val serialVersionUID = 6529685098267757690L

        /**
         * The value of [rumbleDeviceDescriptor] when the built-in Vibrator should be utilized
         */
        const val BuiltinRumbleDeviceDescriptor = "builtin"
    }
}

/**
 * This Controller class is for the Handheld-ProCon controller that change based on the operation mode
 */
class HandheldController(id : Int) : Controller(id, ControllerType.HandheldProController) {
    companion object {
        private val serialVersionUID = Controller.serialVersionUID
    }
}

/**
 * This Controller class is for the Pro Controller (ProCon)
 */
class ProController(id : Int) : Controller(id, ControllerType.ProController) {
    companion object {
        private val serialVersionUID = Controller.serialVersionUID
    }
}

/**
 * This Controller class is for the left Joy-Con controller
 *
 * @param partnerId The ID of the corresponding right Joy-Con if this is a pair
 */
class JoyConLeftController(id : Int, var partnerId : Int? = null) : Controller(id, ControllerType.JoyConLeft) {
    companion object {
        private val serialVersionUID = Controller.serialVersionUID
    }
}

/**
 * This Controller class is for the right Joy-Con controller
 *
 * @param partnerId The ID of the corresponding left Joy-Con if this is a pair
 */
class JoyConRightController(id : Int, var partnerId : Int? = null) : Controller(id, ControllerType.JoyConRight) {
    companion object {
        private val serialVersionUID = Controller.serialVersionUID
    }
}
