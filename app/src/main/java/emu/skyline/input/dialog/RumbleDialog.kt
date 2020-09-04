/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.dialog

import android.animation.LayoutTransition
import android.content.Context
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.*
import android.view.animation.LinearInterpolator
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.R
import emu.skyline.adapter.ControllerGeneralItem
import emu.skyline.input.ControllerActivity
import kotlinx.android.synthetic.main.rumble_dialog.*

/**
 * This dialog is used to set a device to pass on any rumble/force feedback data onto
 *
 * @param item This is used to hold the [ControllerGeneralItem] between instances
 */
class RumbleDialog(val item : ControllerGeneralItem) : BottomSheetDialogFragment() {
    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? = inflater.inflate(R.layout.rumble_dialog, container)

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    /**
     * This sets up all user interaction with this dialog
     */
    override fun onActivityCreated(savedInstanceState : Bundle?) {
        super.onActivityCreated(savedInstanceState)

        if (context is ControllerActivity) {
            val context = requireContext() as ControllerActivity
            val controller = context.manager.controllers[context.id]!!

            // Set up the reset button to clear out [Controller.rumbleDevice] when pressed
            rumble_reset.setOnClickListener {
                controller.rumbleDeviceDescriptor = null
                controller.rumbleDeviceName = null
                item.update()

                dismiss()
            }

            if (context.id == 0) {
                rumble_builtin.visibility = View.VISIBLE
                if (!(context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator).hasVibrator())
                    rumble_builtin.isEnabled = false
                rumble_builtin.setOnClickListener {
                    controller.rumbleDeviceDescriptor = "builtin"
                    controller.rumbleDeviceName = getString(R.string.builtin_vibrator)
                    item.update()

                    dismiss()
                }
            }

            // Ensure that layout animations are proper
            rumble_layout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)
            rumble_controller.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            var deviceId : Int? = null // The ID of the currently selected device

            dialog?.setOnKeyListener { _, _, event ->
                // We want all input events from Joysticks and game pads
                if (event.isFromSource(InputDevice.SOURCE_GAMEPAD) || event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
                    if (event.repeatCount == 0 && event.action == KeyEvent.ACTION_DOWN) {
                        val vibrator = event.device.vibrator

                        when {
                            // If the device doesn't match the currently selected device then update the UI accordingly and set [deviceId] to the current device
                            deviceId != event.deviceId -> {
                                rumble_controller_name.text = event.device.name

                                if (vibrator.hasVibrator()) {
                                    rumble_controller_supported.text = getString(R.string.supported)
                                    rumble_title.text = getString(R.string.confirm_button_again)

                                    vibrator.vibrate(VibrationEffect.createOneShot(250, VibrationEffect.DEFAULT_AMPLITUDE))
                                } else {
                                    rumble_controller_supported.text = getString(R.string.not_supported)
                                    dialog?.setOnKeyListener { _, _, _ -> false }
                                    rumble_reset.requestFocus()
                                }

                                rumble_controller_icon.animate().apply {
                                    interpolator = LinearInterpolator()
                                    duration = 100
                                    alpha(if (vibrator.hasVibrator()) 0.75f else 0.5f)
                                    start()
                                }

                                deviceId = event.deviceId
                            }

                            // If the currently selected device has a vibrator then go ahead and select it
                            vibrator.hasVibrator() -> {
                                vibrator.vibrate(VibrationEffect.createOneShot(250, VibrationEffect.DEFAULT_AMPLITUDE))

                                controller.rumbleDeviceDescriptor = event.device.descriptor
                                controller.rumbleDeviceName = event.device.name

                                item.update()

                                dismiss()
                            }

                            else -> {
                                return@setOnKeyListener false
                            }
                        }
                    }

                    true
                } else {
                    false
                }
            }
        } else {
            dismiss()
        }
    }
}
