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
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.R
import emu.skyline.adapter.ControllerButtonItem
import emu.skyline.input.*
import kotlinx.android.synthetic.main.button_dialog.*
import kotlin.math.abs

/**
 * This dialog is used to set a device to map any buttons
 *
 * @param item This is used to hold the [ControllerButtonItem] between instances
 */
class ButtonDialog(val item : ControllerButtonItem) : BottomSheetDialogFragment() {
    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? = inflater.inflate(R.layout.button_dialog, container)

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

            // View focus handling so all input is always directed to this view
            view?.requestFocus()
            view?.onFocusChangeListener = View.OnFocusChangeListener { v, hasFocus -> if (!hasFocus) v.requestFocus() }

            // Write the text for the button's icon
            button_text.text = item.button.short ?: item.button.toString()

            // Set up the reset button to clear out all entries corresponding to this button from [InputManager.eventMap]
            button_reset.setOnClickListener {
                val guestEvent = ButtonGuestEvent(context.id, item.button)

                context.manager.eventMap.filterValues { it is ButtonGuestEvent && it == guestEvent }.keys.forEach { context.manager.eventMap.remove(it) }

                item.update()

                dismiss()
            }

            // Ensure that layout animations are proper
            button_layout.layoutTransition.setAnimateParentHierarchy(false)
            button_layout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            // We want the secondary progress bar to be visible through the first one
            button_seekbar.progressDrawable.alpha = 128

            var deviceId : Int? = null // The ID of the currently selected device
            var inputId : Int? = null // The key code/axis ID of the currently selected event

            var axisPolarity = false // The polarity of the axis for the currently selected event
            var axisRunnable : Runnable? = null // The Runnable that is used for counting down till an axis is selected
            val axisHandler = Handler(Looper.getMainLooper()) // The handler responsible for handling posting [axisRunnable]

            // The last values of the HAT axes so that they can be ignored in [View.OnGenericMotionListener] so they are passed onto [DialogInterface.OnKeyListener] as [KeyEvent]s
            var oldDpadX = 0.0f
            var oldDpadY = 0.0f

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

                        button_icon.animate().alpha(0.75f).setDuration(50).start()
                        button_text.animate().alpha(0.9f).setDuration(50).start()

                        button_title.text = getString(R.string.release_confirm)
                        button_seekbar.visibility = View.GONE
                    } else if (deviceId == event.deviceId && inputId == event.keyCode && event.action == KeyEvent.ACTION_UP) {
                        // We serialize the current [deviceId] and [inputId] into a [KeyHostEvent] and map it to a corresponding [GuestEvent] on [KeyEvent.ACTION_UP]
                        val hostEvent = KeyHostEvent(event.device.descriptor, event.keyCode)

                        var guestEvent = context.manager.eventMap[hostEvent]

                        if (guestEvent is GuestEvent) {
                            context.manager.eventMap.remove(hostEvent)

                            if (guestEvent is ButtonGuestEvent)
                                context.buttonMap[guestEvent.button]?.update()
                            else if (guestEvent is AxisGuestEvent)
                                context.axisMap[guestEvent.axis]?.update()
                        }

                        guestEvent = ButtonGuestEvent(context.id, item.button)

                        context.manager.eventMap.filterValues { it == guestEvent }.keys.forEach { context.manager.eventMap.remove(it) }

                        context.manager.eventMap[hostEvent] = guestEvent

                        item.update()

                        dismiss()
                    }

                    true
                } else {
                    false
                }
            }

            val axes = arrayOf(MotionEvent.AXIS_X, MotionEvent.AXIS_Y, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, MotionEvent.AXIS_HAT_X, MotionEvent.AXIS_HAT_Y, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_THROTTLE, MotionEvent.AXIS_RUDDER, MotionEvent.AXIS_WHEEL, MotionEvent.AXIS_GAS, MotionEvent.AXIS_BRAKE).plus(IntRange(MotionEvent.AXIS_GENERIC_1, MotionEvent.AXIS_GENERIC_16).toList())

            val axesHistory = arrayOfNulls<Float>(axes.size) // The last recorded value of an axis, this is used to eliminate any stagnant axes

            view?.setOnGenericMotionListener { _, event ->
                // We retrieve the value of the HAT axes so that we can check for change and ignore any input from them so it'll be passed onto the [KeyEvent] handler
                val dpadX = event.getAxisValue(MotionEvent.AXIS_HAT_X)
                val dpadY = event.getAxisValue(MotionEvent.AXIS_HAT_Y)

                // We want all input events from Joysticks and Buttons that are [MotionEvent.ACTION_MOVE] and not from the D-pad
                if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE && dpadX == oldDpadX && dpadY == oldDpadY) {
                    // We iterate over every axis to check if any of them pass the selection threshold and if they do then select them by setting [deviceId], [inputId] and [axisPolarity]
                    for (axisItem in axes.withIndex()) {
                        val axis = axisItem.value
                        val value = event.getAxisValue(axis)

                        // This checks the history of the axis so it we can ignore any stagnant axis
                        if ((event.historySize == 0 || value == event.getHistoricalAxisValue(axis, 0)) && (axesHistory[axisItem.index]?.let { it == value } != false)) {
                            axesHistory[axisItem.index] = value
                            continue
                        }

                        axesHistory[axisItem.index] = value

                        if (abs(value) >= 0.5 && (deviceId != event.deviceId || inputId != axis || axisPolarity != (value >= 0)) && !(axes.contains(inputId) && value == event.getAxisValue(inputId!!))) {
                            deviceId = event.deviceId
                            inputId = axis
                            axisPolarity = value >= 0

                            button_title.text = getString(R.string.hold_confirm)
                            button_seekbar.visibility = View.VISIBLE

                            break
                        }
                    }

                    // If the currently active input is a valid axis
                    if (axes.contains(inputId)) {
                        val value = event.getAxisValue(inputId!!)
                        val threshold = button_seekbar.progress / 100f

                        // Update the secondary progress bar in [button_seekbar] based on the axis's value
                        button_seekbar.secondaryProgress = (abs(value) * 100).toInt()

                        // If the axis value crosses the threshold then post [axisRunnable] with a delay and animate the views accordingly
                        if (abs(value) >= threshold) {
                            if (axisRunnable == null) {
                                axisRunnable = Runnable {
                                    val hostEvent = MotionHostEvent(event.device.descriptor, inputId!!, axisPolarity)

                                    var guestEvent = context.manager.eventMap[hostEvent]

                                    if (guestEvent is GuestEvent) {
                                        context.manager.eventMap.remove(hostEvent)

                                        if (guestEvent is ButtonGuestEvent)
                                            context.buttonMap[(guestEvent as ButtonGuestEvent).button]?.update()
                                        else if (guestEvent is AxisGuestEvent)
                                            context.axisMap[(guestEvent as AxisGuestEvent).axis]?.update()
                                    }

                                    guestEvent = ButtonGuestEvent(controller.id, item.button, threshold)

                                    context.manager.eventMap.filterValues { it == guestEvent }.keys.forEach { context.manager.eventMap.remove(it) }

                                    context.manager.eventMap[hostEvent] = guestEvent

                                    item.update()

                                    dismiss()
                                }

                                axisHandler.postDelayed(axisRunnable!!, 1000)
                            }

                            button_icon.animate().alpha(0.85f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                            button_text.animate().alpha(0.95f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                        } else {
                            // If the axis value is below the threshold, remove [axisRunnable] from it being posted and animate the views accordingly
                            if (axisRunnable != null) {
                                axisHandler.removeCallbacks(axisRunnable!!)
                                axisRunnable = null
                            }

                            button_icon.animate().alpha(0.25f).setDuration(50).start()
                            button_text.animate().alpha(0.35f).setDuration(50).start()
                        }
                    }

                    true
                } else {
                    oldDpadX = dpadX
                    oldDpadY = dpadY

                    false
                }
            }
        } else {
            dismiss()
        }
    }
}
