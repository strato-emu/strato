/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input.dialog

import android.animation.LayoutTransition
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.*
import android.view.animation.LinearInterpolator
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.R
import emu.skyline.adapter.ControllerStickItem
import emu.skyline.input.*
import emu.skyline.input.MotionHostEvent.Companion.axes
import kotlinx.android.synthetic.main.stick_dialog.*
import java.util.*
import kotlin.math.abs
import kotlin.math.max

/**
 * This dialog is used to set a device to map any sticks
 *
 * @param item This is used to hold the [ControllerStickItem] between instances
 */
class StickDialog @JvmOverloads constructor(val item : ControllerStickItem? = null) : BottomSheetDialogFragment() {
    /**
     * This enumerates all of the stages this dialog can be in
     */
    private enum class DialogStage(val string : Int) {
        Button(R.string.stick_button),
        YPlus(R.string.y_plus),
        YMinus(R.string.y_minus),
        XMinus(R.string.x_minus),
        XPlus(R.string.x_plus),
        Stick(R.string.stick_preview);
    }

    /**
     * This is the current stage of the dialog
     */
    private var stage = DialogStage.Button

    /**
     * This is the handler of all [Runnable]s posted by the dialog
     */
    private val handler = Handler(Looper.getMainLooper())

    /**
     * This is the [Runnable] that is used for running the current stage's animation
     */
    private var stageAnimation : Runnable? = null

    /**
     * This is a flag that causes any running animation to immediately halt
     */
    private var animationStop = false

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? {
        return requireActivity().layoutInflater.inflate(R.layout.stick_dialog, container)
    }

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    /**
     * This function converts [dip] (Density Independent Pixels) to normal pixels
     */
    private fun dipToPixels(dip : Float) = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dip, resources.displayMetrics)

    /**
     * This function updates the animation based on the current stage and stops the currently running animation if it hasn't already
     */
    @Suppress("LABEL_NAME_CLASH")
    fun updateAnimation() {
        animationStop = false
        stageAnimation?.let { handler.removeCallbacks(it) }

        stick_container?.animate()?.scaleX(1f)?.scaleY(1f)?.alpha(1f)?.translationY(0f)?.translationX(0f)?.rotationX(0f)?.rotationY(0f)?.start()

        when (stage) {
            DialogStage.Button -> {
                stageAnimation = Runnable {
                    if (stage != DialogStage.Button || animationStop)
                        return@Runnable

                    stick_container?.animate()?.scaleX(0.85f)?.scaleY(0.85f)?.alpha(1f)?.withEndAction {
                        if (stage != DialogStage.Button || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if (stage != DialogStage.Button || animationStop)
                                return@Runnable

                            stick_container?.animate()?.scaleX(1f)?.scaleY(1f)?.alpha(0.85f)?.withEndAction {
                                if (stage != DialogStage.Button || animationStop)
                                    return@withEndAction

                                stageAnimation?.let {
                                    handler.postDelayed(it, 750)
                                }
                            }?.start()
                        }

                        handler.postDelayed(runnable, 300)
                    }?.start()
                }
            }

            DialogStage.YPlus, DialogStage.YMinus -> {
                val polarity = if (stage == DialogStage.YMinus) 1 else -1

                stageAnimation = Runnable {
                    if ((stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                        return@Runnable

                    stick_container?.animate()?.setDuration(300)?.translationY(dipToPixels(15f) * polarity)?.rotationX(27f * polarity)?.alpha(1f)?.withEndAction {
                        if ((stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if ((stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                                return@Runnable

                            stick_container?.animate()?.setDuration(250)?.translationY(0f)?.rotationX(0f)?.alpha(0.85f)?.withEndAction {
                                if ((stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                                    return@withEndAction

                                stageAnimation?.let {
                                    handler.postDelayed(it, 750)
                                }
                            }?.start()
                        }

                        handler.postDelayed(runnable, 300)
                    }?.start()
                }
            }

            DialogStage.XPlus, DialogStage.XMinus -> {
                val polarity = if (stage == DialogStage.XPlus) 1 else -1

                stageAnimation = Runnable {
                    if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                        return@Runnable

                    stick_container?.animate()?.setDuration(300)?.translationX(dipToPixels(16.5f) * polarity)?.rotationY(27f * polarity)?.alpha(1f)?.withEndAction {
                        if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                                return@Runnable

                            stick_container?.animate()?.setDuration(250)?.translationX(0f)?.rotationY(0f)?.alpha(0.85f)?.withEndAction {
                                if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                                    return@withEndAction

                                stageAnimation?.let {
                                    handler.postDelayed(it, 750)
                                }
                            }?.start()
                        }

                        handler.postDelayed(runnable, 300)
                    }?.start()
                }
            }

            else -> {
            }
        }

        stageAnimation?.let { handler.postDelayed(it, 750) }
    }

    /**
     * This function goes to a particular stage based on the offset from the current stage
     */
    private fun gotoStage(offset : Int = 1) {
        val ordinal = stage.ordinal + offset
        val size = DialogStage.values().size

        if (ordinal in 0 until size) {
            stage = DialogStage.values()[ordinal]

            stick_title.text = getString(stage.string)
            stick_subtitle.text = if (stage != DialogStage.Stick) getString(R.string.use_button_axis) else getString(R.string.use_non_stick)
            stick_icon.animate().alpha(0.25f).setDuration(50).start()
            stick_name.animate().alpha(0.35f).setDuration(50).start()
            stick_seekbar.visibility = View.GONE

            stick_next.text = if (ordinal + 1 == size) getString(R.string.done) else getString(R.string.next)

            updateAnimation()
        } else {
            dismiss()
        }
    }

    /**
     * This sets up all user interaction with this dialog
     */
    override fun onActivityCreated(savedInstanceState : Bundle?) {
        super.onActivityCreated(savedInstanceState)

        if (item != null && context is ControllerActivity) {
            val context = requireContext() as ControllerActivity
            val controller = InputManager.controllers[context.id]!!

            // View focus handling so all input is always directed to this view
            view?.requestFocus()
            view?.onFocusChangeListener = View.OnFocusChangeListener { v, hasFocus -> if (!hasFocus) v.requestFocus() }

            // Write the text for the stick's icon
            stick_name.text = item.stick.button.short ?: item.stick.button.toString()

            // Set up the reset button to clear out all entries corresponding to this stick from [InputManager.eventMap]
            stick_reset.setOnClickListener {
                for (axis in arrayOf(item.stick.xAxis, item.stick.yAxis)) {
                    for (polarity in booleanArrayOf(true, false)) {
                        val guestEvent = AxisGuestEvent(context.id, axis, polarity)

                        InputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { InputManager.eventMap.remove(it) }
                    }
                }

                val guestEvent = ButtonGuestEvent(context.id, item.stick.button)

                InputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { InputManager.eventMap.remove(it) }

                item.update()

                dismiss()
            }

            // Ensure that layout animations are proper
            stick_layout.layoutTransition.setAnimateParentHierarchy(false)
            stick_layout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            // We want the secondary progress bar to be visible through the first one
            stick_seekbar.progressDrawable.alpha = 128

            updateAnimation()

            var deviceId : Int? = null // The ID of the currently selected device
            var inputId : Int? = null // The key code/axis ID of the currently selected event

            val ignoredEvents = mutableListOf<Int>() // The hashes of events that are to be ignored due to being already mapped to some component of the stick

            var axisPolarity = false // The polarity of the axis for the currently selected event
            var axisRunnable : Runnable? = null // The Runnable that is used for counting down till an axis is selected

            stick_next.setOnClickListener {
                gotoStage(1)

                deviceId = null
                inputId = null

                axisRunnable?.let { handler.removeCallbacks(it) }
            }

            view?.setOnKeyListener { _, _, event ->
                when {
                    // We want all input events from Joysticks and Buttons except for [KeyEvent.KEYCODE_BACK] as that will should be processed elsewhere
                    ((event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON) && event.keyCode != KeyEvent.KEYCODE_BACK) || event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) && event.repeatCount == 0 -> {
                        if (stage == DialogStage.Stick) {
                            // When the stick is being previewed after everything is mapped we do a lookup into [InputManager.eventMap] to find a corresponding [GuestEvent] and animate the stick correspondingly
                            when (val guestEvent = InputManager.eventMap[KeyHostEvent(event.device.descriptor, event.keyCode)]) {
                                is ButtonGuestEvent -> {
                                    if (guestEvent.button == item.stick.button) {
                                        if (event.action == KeyEvent.ACTION_DOWN) {
                                            stick_container?.animate()?.setStartDelay(0)?.setDuration(50)?.scaleX(0.85f)?.scaleY(0.85f)?.start()

                                            stick_icon.animate().alpha(0.85f).setDuration(50).start()
                                            stick_name.animate().alpha(0.95f).setDuration(50).start()
                                        } else {
                                            stick_container?.animate()?.setStartDelay(0)?.setDuration(25)?.scaleX(1f)?.scaleY(1f)?.start()

                                            stick_icon.animate().alpha(0.25f).setDuration(25).start()
                                            stick_name.animate().alpha(0.35f).setDuration(25).start()
                                        }
                                    } else if (event.action == KeyEvent.ACTION_UP) {
                                        stick_next?.callOnClick()
                                    }
                                }

                                is AxisGuestEvent -> {
                                    val coefficient = if (event.action == KeyEvent.ACTION_DOWN) if (guestEvent.polarity) 1 else -1 else 0

                                    if (guestEvent.axis == item.stick.xAxis) {
                                        stick_container?.translationX = dipToPixels(16.5f) * coefficient
                                        stick_container?.rotationY = 27f * coefficient
                                    } else if (guestEvent.axis == item.stick.yAxis) {
                                        stick_container?.translationY = dipToPixels(16.5f) * -coefficient
                                        stick_container?.rotationX = 27f * coefficient
                                    }
                                }

                                null -> if (event.action == KeyEvent.ACTION_UP) stick_next?.callOnClick()
                            }
                        } else if (stage != DialogStage.Stick) {
                            if ((deviceId != event.deviceId || inputId != event.keyCode) && event.action == KeyEvent.ACTION_DOWN && !ignoredEvents.any { it == Objects.hash(event.deviceId, event.keyCode) }) {
                                // We set [deviceId] and [inputId] on [KeyEvent.ACTION_DOWN] alongside updating the views to match the action while ignoring any events in [ignoredEvents]
                                deviceId = event.deviceId
                                inputId = event.keyCode

                                if (axisRunnable != null) {
                                    handler.removeCallbacks(axisRunnable!!)
                                    axisRunnable = null
                                }

                                animationStop = true

                                val coefficient = if (stage == DialogStage.YMinus || stage == DialogStage.XPlus) 1 else -1

                                when (stage) {
                                    DialogStage.Button -> stick_container?.animate()?.setStartDelay(0)?.setDuration(25)?.scaleX(0.85f)?.scaleY(0.85f)?.alpha(1f)?.start()
                                    DialogStage.YPlus, DialogStage.YMinus -> stick_container?.animate()?.setStartDelay(0)?.setDuration(75)?.translationY(dipToPixels(16.5f) * coefficient)?.rotationX(27f * coefficient)?.alpha(1f)?.start()
                                    DialogStage.XPlus, DialogStage.XMinus -> stick_container?.animate()?.setStartDelay(0)?.setDuration(75)?.translationX(dipToPixels(16.5f) * coefficient)?.rotationY(27f * coefficient)?.alpha(1f)?.start()
                                    else -> {
                                    }
                                }

                                stick_icon.animate().alpha(0.85f).setDuration(50).start()
                                stick_name.animate().alpha(0.95f).setDuration(50).start()

                                stick_subtitle.text = getString(R.string.release_confirm)
                                stick_seekbar.visibility = View.GONE
                            } else if (deviceId == event.deviceId && inputId == event.keyCode && event.action == KeyEvent.ACTION_UP) {
                                // We serialize the current [deviceId] and [inputId] into a [KeyHostEvent] and map it to a corresponding [GuestEvent] and add it to [ignoredEvents] on [KeyEvent.ACTION_UP]
                                val hostEvent = KeyHostEvent(event.device.descriptor, event.keyCode)

                                var guestEvent = InputManager.eventMap[hostEvent]

                                if (guestEvent is GuestEvent) {
                                    InputManager.eventMap.remove(hostEvent)

                                    if (guestEvent is ButtonGuestEvent)
                                        context.buttonMap[guestEvent.button]?.update()
                                    else if (guestEvent is AxisGuestEvent)
                                        context.axisMap[guestEvent.axis]?.update()
                                }

                                guestEvent = when (stage) {
                                    DialogStage.Button -> ButtonGuestEvent(controller.id, item.stick.button)
                                    DialogStage.YPlus, DialogStage.YMinus -> AxisGuestEvent(controller.id, item.stick.yAxis, stage == DialogStage.YPlus)
                                    DialogStage.XPlus, DialogStage.XMinus -> AxisGuestEvent(controller.id, item.stick.xAxis, stage == DialogStage.XPlus)
                                    else -> null
                                }

                                InputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { InputManager.eventMap.remove(it) }

                                InputManager.eventMap[hostEvent] = guestEvent

                                ignoredEvents.add(Objects.hash(deviceId!!, inputId!!))

                                item.update()

                                stick_next?.callOnClick()
                            }
                        }

                        true
                    }

                    event.keyCode == KeyEvent.KEYCODE_BACK && event.action == KeyEvent.ACTION_UP -> {
                        // We handle [KeyEvent.KEYCODE_BACK] by trying to go to the last stage using [gotoStage]
                        gotoStage(-1)

                        deviceId = null
                        inputId = null

                        true
                    }

                    else -> {
                        false
                    }
                }
            }

            val axesHistory = arrayOfNulls<Float>(axes.size) // The last recorded value of an axis, this is used to eliminate any stagnant axes
            val axesMax = Array(axes.size) { 0f } // The maximum recorded value of the axis, this is to scale the axis to a stick accordingly (The value is also checked at runtime, so it's fine if this isn't the true maximum)

            var oldHat = Pair(0.0f, 0.0f) // The last values of the HAT axes so that they can be ignored in [View.OnGenericMotionListener] so they are passed onto [DialogInterface.OnKeyListener] as [KeyEvent]s

            view?.setOnGenericMotionListener { _, event ->
                // We retrieve the value of the HAT axes so that we can check for change and ignore any input from them so it'll be passed onto the [KeyEvent] handler
                val hat = Pair(event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y))

                // We want all input events from Joysticks and Buttons that are [MotionEvent.ACTION_MOVE] and not from the D-pad
                if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE && hat == oldHat) {
                    if (stage == DialogStage.Stick) {
                        // When the stick is being previewed after everything is mapped we do a lookup into [InputManager.eventMap] to find a corresponding [GuestEvent] and animate the stick correspondingly
                        for (axisItem in axes.withIndex()) {
                            val axis = axisItem.value
                            var value = event.getAxisValue(axis)

                            if ((event.historySize == 0 || value == event.getHistoricalAxisValue(axis, 0)) && (axesHistory[axisItem.index]?.let { it == value } != false)) {
                                axesHistory[axisItem.index] = value
                                continue
                            }

                            axesHistory[axisItem.index] = value

                            var polarity = value >= 0
                            val guestEvent = MotionHostEvent(event.device.descriptor, axis, polarity).let { hostEvent ->
                                InputManager.eventMap[hostEvent] ?: if (value == 0f) {
                                    polarity = false
                                    InputManager.eventMap[hostEvent.copy(polarity = false)]
                                } else {
                                    null
                                }
                            }

                            when (guestEvent) {
                                is ButtonGuestEvent -> {
                                    if (guestEvent.button == item.stick.button) {
                                        if (abs(value) >= guestEvent.threshold) {
                                            stick_container?.animate()?.setStartDelay(0)?.setDuration(50)?.scaleX(0.85f)?.scaleY(0.85f)?.start()
                                            stick_icon.animate().alpha(0.85f).setDuration(50).start()
                                            stick_name.animate().alpha(0.95f).setDuration(50).start()
                                        } else {
                                            stick_container?.animate()?.setStartDelay(0)?.setDuration(25)?.scaleX(1f)?.scaleY(1f)?.start()
                                            stick_icon.animate().alpha(0.25f).setDuration(25).start()
                                            stick_name.animate().alpha(0.35f).setDuration(25).start()
                                        }
                                    }
                                }

                                is AxisGuestEvent -> {
                                    value = guestEvent.value(value)

                                    val coefficient = if (polarity) abs(value) else -abs(value)

                                    if (guestEvent.axis == item.stick.xAxis) {
                                        stick_container?.translationX = dipToPixels(16.5f) * coefficient
                                        stick_container?.rotationY = 27f * coefficient
                                    } else if (guestEvent.axis == item.stick.yAxis) {
                                        stick_container?.translationY = dipToPixels(16.5f) * coefficient
                                        stick_container?.rotationX = 27f * -coefficient
                                    }
                                }
                            }
                        }
                    } else {
                        // We iterate over every axis to check if any of them pass the selection threshold and if they do then select them by setting [deviceId], [inputId] and [axisPolarity]
                        for (axisItem in axes.withIndex()) {
                            val axis = axisItem.value
                            val value = event.getAxisValue(axis)

                            axesMax[axisItem.index] = max(abs(value), axesMax[axisItem.index])

                            // This checks the history of the axis so it we can ignore any stagnant axis
                            if ((event.historySize == 0 || value == event.getHistoricalAxisValue(axis, 0)) && (axesHistory[axisItem.index]?.let { it == value } != false)) {
                                axesHistory[axisItem.index] = value
                                continue
                            }

                            axesHistory[axisItem.index] = value

                            if (abs(value) >= 0.5 && (deviceId != event.deviceId || inputId != axis || axisPolarity != (value >= 0)) && !ignoredEvents.any { it == Objects.hash(event.deviceId, axis, value >= 0) } && !(axes.contains(inputId) && value == event.getAxisValue(inputId!!))) {
                                deviceId = event.deviceId
                                inputId = axis
                                axisPolarity = value >= 0

                                stick_subtitle.text = getString(R.string.hold_confirm)

                                if (stage == DialogStage.Button)
                                    stick_seekbar.visibility = View.VISIBLE

                                animationStop = true

                                break
                            }
                        }

                        // If the currently active input is a valid axis
                        if (axes.contains(inputId)) {
                            val value = event.getAxisValue(inputId!!)
                            val threshold = if (stage == DialogStage.Button) stick_seekbar.progress / 100f else 0.5f

                            when (stage) {
                                // Update the secondary progress bar in [button_seekbar] based on the axis's value
                                DialogStage.Button -> {
                                    stick_container?.animate()?.setStartDelay(0)?.setDuration(25)?.scaleX(0.85f)?.scaleY(0.85f)?.alpha(1f)?.start()
                                    stick_seekbar.secondaryProgress = (abs(value) * 100).toInt()
                                }


                                // Update the the position of the stick in the Y-axis based on the axis's value
                                DialogStage.YPlus, DialogStage.YMinus -> {
                                    val coefficient = if (stage == DialogStage.YMinus) abs(value) else -abs(value)

                                    stick_container?.translationY = dipToPixels(16.5f) * coefficient
                                    stick_container?.rotationX = 27f * -coefficient
                                }

                                // Update the the position of the stick in the X-axis based on the axis's value
                                DialogStage.XPlus, DialogStage.XMinus -> {
                                    val coefficient = if (stage == DialogStage.XPlus) abs(value) else -abs(value)

                                    stick_container?.translationX = dipToPixels(16.5f) * coefficient
                                    stick_container?.rotationY = 27f * coefficient
                                }

                                else -> {
                                }
                            }

                            // If the axis value crosses the threshold then post [axisRunnable] with a delay and animate the views accordingly
                            if (abs(value) >= threshold) {
                                if (axisRunnable == null) {
                                    axisRunnable = Runnable {
                                        val hostEvent = MotionHostEvent(event.device.descriptor, inputId!!, axisPolarity)

                                        var guestEvent = InputManager.eventMap[hostEvent]

                                        if (guestEvent is GuestEvent) {
                                            InputManager.eventMap.remove(hostEvent)

                                            if (guestEvent is ButtonGuestEvent)
                                                context.buttonMap[(guestEvent as ButtonGuestEvent).button]?.update()
                                            else if (guestEvent is AxisGuestEvent)
                                                context.axisMap[(guestEvent as AxisGuestEvent).axis]?.update()
                                        }

                                        val max = axesMax[axes.indexOf(inputId!!)]

                                        guestEvent = when (stage) {
                                            DialogStage.Button -> ButtonGuestEvent(controller.id, item.stick.button, threshold)
                                            DialogStage.YPlus, DialogStage.YMinus -> AxisGuestEvent(controller.id, item.stick.yAxis, stage == DialogStage.YPlus, max)
                                            DialogStage.XPlus, DialogStage.XMinus -> AxisGuestEvent(controller.id, item.stick.xAxis, stage == DialogStage.XPlus, max)
                                            else -> null
                                        }

                                        InputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { InputManager.eventMap.remove(it) }

                                        InputManager.eventMap[hostEvent] = guestEvent

                                        ignoredEvents.add(Objects.hash(deviceId!!, inputId!!, axisPolarity))

                                        axisRunnable = null

                                        item.update()

                                        stick_next?.callOnClick()
                                    }

                                    handler.postDelayed(axisRunnable!!, 1000)
                                }

                                stick_icon.animate().alpha(0.85f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                                stick_name.animate().alpha(0.95f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                            } else {
                                // If the axis value is below the threshold, remove [axisRunnable] from it being posted and animate the views accordingly
                                if (axisRunnable != null) {
                                    handler.removeCallbacks(axisRunnable!!)
                                    axisRunnable = null
                                }

                                if (stage == DialogStage.Button)
                                    stick_container?.animate()?.setStartDelay(0)?.setDuration(10)?.scaleX(1f)?.scaleY(1f)?.alpha(1f)?.start()

                                stick_icon.animate().alpha(0.25f).setDuration(50).start()
                                stick_name.animate().alpha(0.35f).setDuration(50).start()
                            }
                        }
                    }

                    true
                } else {
                    oldHat = hat

                    false
                }
            }
        } else {
            dismiss()
        }
    }
}
