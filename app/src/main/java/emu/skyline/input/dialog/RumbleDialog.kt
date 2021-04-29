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
import emu.skyline.adapter.controller.ControllerGeneralViewItem
import emu.skyline.databinding.RumbleDialogBinding
import emu.skyline.di.getInputManager
import emu.skyline.input.ControllerActivity

/**
 * This dialog is used to set a device to pass on any rumble/force feedback data onto
 *
 * @param item This is used to hold the [ControllerGeneralViewItem] between instances
 */
class RumbleDialog @JvmOverloads constructor(val item : ControllerGeneralViewItem? = null) : BottomSheetDialogFragment() {
    private var _binding : RumbleDialogBinding? = null
    private val binding get() = _binding!!

    private val inputManager by lazy { requireContext().getInputManager() }

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = RumbleDialogBinding.inflate(inflater).also { _binding = it }.root

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        if (item != null && context is ControllerActivity) {
            val context = requireContext() as ControllerActivity
            val controller = inputManager.controllers[context.id]!!

            // Set up the reset button to clear out [Controller.rumbleDevice] when pressed
            binding.rumbleReset.setOnClickListener {
                controller.rumbleDeviceDescriptor = null
                controller.rumbleDeviceName = null
                item.update()

                dismiss()
            }

            if (context.id == 0) {
                binding.rumbleBuiltin.visibility = View.VISIBLE
                if (!(context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator).hasVibrator())
                    binding.rumbleBuiltin.isEnabled = false
                binding.rumbleBuiltin.setOnClickListener {
                    controller.rumbleDeviceDescriptor = "builtin"
                    controller.rumbleDeviceName = getString(R.string.builtin_vibrator)
                    item.update()

                    dismiss()
                }
            }

            // Ensure that layout animations are proper
            binding.rumbleLayout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)
            binding.rumbleController.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            var deviceId : Int? = null // The ID of the currently selected device

            dialog?.setOnKeyListener { _, _, event ->
                // We want all input events from Joysticks and game pads
                if (event.isFromSource(InputDevice.SOURCE_GAMEPAD) || event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
                    if (event.repeatCount == 0 && event.action == KeyEvent.ACTION_DOWN) {
                        val vibrator = event.device.vibrator

                        when {
                            // If the device doesn't match the currently selected device then update the UI accordingly and set [deviceId] to the current device
                            deviceId != event.deviceId -> {
                                binding.rumbleControllerName.text = event.device.name

                                if (vibrator.hasVibrator()) {
                                    binding.rumbleControllerSupported.text = getString(R.string.supported)
                                    binding.rumbleTitle.text = getString(R.string.confirm_button_again)

                                    vibrator.vibrate(VibrationEffect.createOneShot(250, VibrationEffect.DEFAULT_AMPLITUDE))
                                } else {
                                    binding.rumbleControllerSupported.text = getString(R.string.not_supported)
                                    dialog?.setOnKeyListener { _, _, _ -> false }
                                    binding.rumbleReset.requestFocus()
                                }

                                binding.rumbleControllerIcon.animate().apply {
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

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
