/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.dialog

import android.animation.LayoutTransition
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.*
import android.view.animation.LinearInterpolator
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.fragment.app.commit
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.R
import emu.skyline.adapter.controller.ControllerButtonViewItem
import emu.skyline.databinding.ButtonDialogBinding
import emu.skyline.di.getInputManager
import emu.skyline.input.*
import kotlin.math.abs

/**
 * This dialog is used to set a device to map any buttons
 *
 * @param item This is used to hold the [ControllerButtonViewItem] between instances
 */
class ButtonDialog @JvmOverloads constructor(private val item : ControllerButtonViewItem? = null, private val nextDialog : BottomSheetDialogFragment? = null) : BottomSheetDialogFragment() {
    private var _binding : ButtonDialogBinding? = null
    private val binding get() = _binding!!

    private val inputManager by lazy { requireContext().getInputManager() }

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = ButtonDialogBinding.inflate(inflater).also { _binding = it }.root

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val parentView = requireView().parent as View
        if (parentView.layoutParams is CoordinatorLayout.LayoutParams) {
            val behavior = BottomSheetBehavior.from(parentView)
            behavior.state = BottomSheetBehavior.STATE_EXPANDED
        }
    }

    private fun gotoNextOrDismiss() {
        if (nextDialog != null) {
            parentFragmentManager.commit {
                remove(this@ButtonDialog)
                add(nextDialog, null)
            }
        } else {
            dismiss()
        }
    }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        if (item != null && context is ControllerActivity) {
            val context = requireContext() as ControllerActivity
            val controller = inputManager.controllers[context.id]!!

            // View focus handling so all input is always directed to this view
            view.requestFocus()
            view.onFocusChangeListener = View.OnFocusChangeListener { v, hasFocus -> if (!hasFocus) v.requestFocus() }

            // Write the text for the button's icon
            binding.buttonText.text = item.button.short ?: item.button.toString()

            // Set up the reset button to clear out all entries corresponding to this button from [inputManager.eventMap]
            binding.buttonReset.setOnClickListener {
                val guestEvent = ButtonGuestEvent(context.id, item.button)

                inputManager.eventMap.filterValues { it is ButtonGuestEvent && it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                item.update()

                gotoNextOrDismiss()
            }

            // Ensure that layout animations are proper
            binding.buttonLayout.layoutTransition.setAnimateParentHierarchy(false)
            binding.buttonLayout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            // We want the secondary progress bar to be visible through the first one
            binding.buttonSeekbar.progressDrawable.alpha = 128

            var deviceId : Int? = null // The ID of the currently selected device
            var inputId : Int? = null // The key code/axis ID of the currently selected event

            var axisPolarity = false // The polarity of the axis for the currently selected event
            var axisRunnable : Runnable? = null // The Runnable that is used for counting down till an axis is selected
            val axisHandler = Handler(Looper.getMainLooper()) // The handler responsible for handling posting [axisRunnable]

            dialog?.setOnKeyListener { _, _, event ->
                // We want all input events from Joysticks and Buttons except for [KeyEvent.KEYCODE_BACK] as that will should be processed elsewhere
                if (((event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON) && event.keyCode != KeyEvent.KEYCODE_BACK) || event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) && event.repeatCount == 0) {
                    if ((deviceId != event.deviceId || inputId != event.keyCode) && event.action == KeyEvent.ACTION_DOWN) {
                        // We set [deviceId] and [inputId] on [KeyEvent.ACTION_DOWN] alongside updating the views to match the action
                        deviceId = event.deviceId
                        inputId = event.keyCode

                        if (axisRunnable != null) {
                            axisHandler.removeCallbacks(axisRunnable!!)
                            axisRunnable = null
                        }

                        binding.buttonIcon.animate().alpha(0.75f).setDuration(50).start()
                        binding.buttonText.animate().alpha(0.9f).setDuration(50).start()

                        binding.buttonTitle.text = getString(R.string.release_confirm)
                        binding.buttonSeekbar.visibility = View.GONE
                    } else if (deviceId == event.deviceId && inputId == event.keyCode && event.action == KeyEvent.ACTION_UP) {
                        // We serialize the current [deviceId] and [inputId] into a [KeyHostEvent] and map it to a corresponding [GuestEvent] on [KeyEvent.ACTION_UP]
                        val hostEvent = KeyHostEvent(event.device.descriptor, event.keyCode)

                        var guestEvent = inputManager.eventMap[hostEvent]

                        if (guestEvent is GuestEvent) {
                            inputManager.eventMap.remove(hostEvent)

                            if (guestEvent is ButtonGuestEvent)
                                context.buttonMap[guestEvent.button]?.update()
                            else if (guestEvent is AxisGuestEvent)
                                context.axisMap[guestEvent.axis]?.update()
                        }

                        guestEvent = ButtonGuestEvent(context.id, item.button)

                        inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                        inputManager.eventMap[hostEvent] = guestEvent

                        item.update()

                        gotoNextOrDismiss()
                    }

                    true
                } else {
                    false
                }
            }

            val axes = MotionHostEvent.axes
            val axesHistory = FloatArray(axes.size) // The last recorded value of an axis, this is used to eliminate any stagnant axes

            view.setOnGenericMotionListener { _, event ->
                // We want all input events from Joysticks and Buttons that are [MotionEvent.ACTION_MOVE] and not from the D-pad
                if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE) {
                    // We iterate over every axis to check if any of them pass the selection threshold and if they do then select them by setting [deviceId], [inputId] and [axisPolarity]
                    for (axisItem in axes.withIndex()) {
                        val axis = axisItem.value
                        val value = event.getAxisValue(axis)

                        // This checks the history of the axis so we can ignore any stagnant axis
                        if ((event.historySize == 0 || value == event.getHistoricalAxisValue(axis, 0)) && axesHistory[axisItem.index] == value) {
                            axesHistory[axisItem.index] = value
                            continue
                        }

                        axesHistory[axisItem.index] = value

                        if (abs(value) >= 0.5 && (deviceId != event.deviceId || inputId != axis || axisPolarity != (value >= 0)) && !(axes.contains(inputId) && value == event.getAxisValue(inputId!!))) {
                            deviceId = event.deviceId
                            inputId = axis
                            axisPolarity = value >= 0

                            binding.buttonTitle.text = getString(R.string.hold_confirm)
                            binding.buttonSeekbar.visibility = View.VISIBLE

                            break
                        }
                    }

                    // If the currently active input is a valid axis
                    if (axes.contains(inputId)) {
                        val value = event.getAxisValue(inputId!!)
                        val threshold = binding.buttonSeekbar.progress / 100f

                        // Update the secondary progress bar in [button_seekbar] based on the axis's value
                        binding.buttonSeekbar.secondaryProgress = (abs(value) * 100).toInt()

                        // If the axis value crosses the threshold then post [axisRunnable] with a delay and animate the views accordingly
                        if (abs(value) >= threshold) {
                            if (axisRunnable == null) {
                                axisRunnable = Runnable {
                                    val hostEvent = MotionHostEvent(event.device.descriptor, inputId!!, axisPolarity)

                                    var guestEvent = inputManager.eventMap[hostEvent]

                                    if (guestEvent is GuestEvent) {
                                        inputManager.eventMap.remove(hostEvent)

                                        if (guestEvent is ButtonGuestEvent)
                                            context.buttonMap[(guestEvent as ButtonGuestEvent).button]?.update()
                                        else if (guestEvent is AxisGuestEvent)
                                            context.axisMap[(guestEvent as AxisGuestEvent).axis]?.update()
                                    }

                                    guestEvent = ButtonGuestEvent(controller.id, item.button, threshold)

                                    inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                                    inputManager.eventMap[hostEvent] = guestEvent

                                    item.update()

                                    gotoNextOrDismiss()
                                }

                                axisHandler.postDelayed(axisRunnable!!, 1000)
                            }

                            binding.buttonIcon.animate().alpha(0.85f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                            binding.buttonText.animate().alpha(0.95f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                        } else {
                            // If the axis value is below the threshold, remove [axisRunnable] from it being posted and animate the views accordingly
                            if (axisRunnable != null) {
                                axisHandler.removeCallbacks(axisRunnable!!)
                                axisRunnable = null
                            }

                            binding.buttonIcon.animate().alpha(0.25f).setDuration(50).start()
                            binding.buttonText.animate().alpha(0.35f).setDuration(50).start()
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
