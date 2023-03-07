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
import androidx.fragment.app.commit
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.R
import emu.skyline.adapter.controller.ControllerStickViewItem
import emu.skyline.databinding.StickDialogBinding
import emu.skyline.di.getInputManager
import emu.skyline.input.*
import emu.skyline.input.MotionHostEvent.Companion.axes
import java.util.*
import kotlin.math.abs
import kotlin.math.max

/**
 * This dialog is used to set a device to map any sticks
 *
 * @param item This is used to hold the [ControllerStickViewItem] between instances
 */
class StickDialog @JvmOverloads constructor(val item : ControllerStickViewItem? = null, private val nextDialog : BottomSheetDialogFragment? = null) : BottomSheetDialogFragment() {
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

    private var _binding : StickDialogBinding? = null
    private val binding get() = _binding!!

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

    private val inputManager by lazy { requireContext().getInputManager() }

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = StickDialogBinding.inflate(inflater).also { _binding = it }.root

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    private fun gotoNextOrDismiss() {
        if (nextDialog != null) {
            parentFragmentManager.commit {
                remove(this@StickDialog)
                add(nextDialog, null)
            }
        } else {
            dismiss()
        }
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

        binding.stickContainer.animate()
            .scaleX(1f).scaleY(1f)
            .alpha(1f)
            .translationY(0f).translationX(0f)
            .rotationX(0f)
            .rotationY(0f)
            .start()

        when (stage) {
            DialogStage.Button -> {
                stageAnimation = Runnable {
                    if (!isAdded || stage != DialogStage.Button || animationStop)
                        return@Runnable

                    binding.stickContainer.animate().scaleX(0.85f).scaleY(0.85f).alpha(1f).withEndAction {
                        if (stage != DialogStage.Button || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if (!isAdded || stage != DialogStage.Button || animationStop)
                                return@Runnable

                            binding.stickContainer.animate().scaleX(1f).scaleY(1f).alpha(0.85f).withEndAction {
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
                    if (!isAdded || (stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                        return@Runnable

                    binding.stickContainer.animate().setDuration(300).translationY(dipToPixels(15f) * polarity).rotationX(27f * polarity).alpha(1f).withEndAction {
                        if ((stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if (!isAdded || (stage != DialogStage.YPlus && stage != DialogStage.YMinus) || animationStop)
                                return@Runnable

                            binding.stickContainer.animate().setDuration(250).translationY(0f).rotationX(0f).alpha(0.85f).withEndAction {
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
                    if (!isAdded || (stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                        return@Runnable

                    binding.stickContainer.animate().setDuration(300).translationX(dipToPixels(16.5f) * polarity).rotationY(27f * polarity).alpha(1f).withEndAction {
                        if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                            return@withEndAction

                        val runnable = Runnable {
                            if (!isAdded || (stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                                return@Runnable

                            binding.stickContainer.animate().setDuration(250).translationX(0f).rotationY(0f).alpha(0.85f).withEndAction {
                                if ((stage != DialogStage.XPlus && stage != DialogStage.XMinus) || animationStop)
                                    return@withEndAction

                                stageAnimation?.let {
                                    handler.postDelayed(it, 750)
                                }
                            }.start()
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

            binding.stickTitle.text = getString(stage.string)
            binding.stickSubtitle.text = getString(if (stage != DialogStage.Stick) R.string.use_button_axis else R.string.use_non_stick)
            binding.stickIcon.animate().alpha(0.25f).setDuration(50).start()
            binding.stickName.animate().alpha(0.35f).setDuration(50).start()
            binding.stickSeekbar.visibility = View.GONE

            binding.stickNext.text = getString(if (ordinal + 1 == size) R.string.done else R.string.next)

            updateAnimation()
        } else if (ordinal == size) {
            gotoNextOrDismiss()
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

            // Write the text for the stick's icon
            binding.stickName.text = item.stick.button.short ?: item.stick.button.toString()

            // Set up the reset button to clear out all entries corresponding to this stick from [inputManager.eventMap]
            binding.stickReset.setOnClickListener {
                for (axis in arrayOf(item.stick.xAxis, item.stick.yAxis)) {
                    for (polarity in booleanArrayOf(true, false)) {
                        val guestEvent = AxisGuestEvent(context.id, axis, polarity)

                        inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }
                    }
                }

                val guestEvent = ButtonGuestEvent(context.id, item.stick.button)

                inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                item.update()

                gotoNextOrDismiss()
            }

            // Ensure that layout animations are proper
            binding.stickLayout.layoutTransition.setAnimateParentHierarchy(false)
            binding.stickLayout.layoutTransition.enableTransitionType(LayoutTransition.CHANGING)

            // We want the secondary progress bar to be visible through the first one
            binding.stickSeekbar.progressDrawable.alpha = 128

            updateAnimation()

            var deviceId : Int? = null // The ID of the currently selected device
            var inputId : Int? = null // The key code/axis ID of the currently selected event

            val ignoredEvents = mutableListOf<Int>() // The hashes of events that are to be ignored due to being already mapped to some component of the stick

            var axisPolarity = false // The polarity of the axis for the currently selected event
            var axisRunnable : Runnable? = null // The Runnable that is used for counting down till an axis is selected

            binding.stickNext.setOnClickListener {
                gotoStage(1)

                deviceId = null
                inputId = null

                axisRunnable?.let { handler.removeCallbacks(it) }
            }

            view.setOnKeyListener { _, _, event ->
                when {
                    // We want all input events from Joysticks and Buttons except for [KeyEvent.KEYCODE_BACK] as that will should be processed elsewhere
                    ((event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON) && event.keyCode != KeyEvent.KEYCODE_BACK) || event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK)) && event.repeatCount == 0 -> {
                        if (stage == DialogStage.Stick) {
                            // When the stick is being previewed after everything is mapped we do a lookup into [inputManager.eventMap] to find a corresponding [GuestEvent] and animate the stick correspondingly
                            when (val guestEvent = inputManager.eventMap[KeyHostEvent(event.device.descriptor, event.keyCode)]) {
                                is ButtonGuestEvent -> {
                                    if (guestEvent.button == item.stick.button) {
                                        if (event.action == KeyEvent.ACTION_DOWN) {
                                            binding.stickContainer.animate().setStartDelay(0).setDuration(50).scaleX(0.85f).scaleY(0.85f).start()

                                            binding.stickIcon.animate().alpha(0.85f).setDuration(50).start()
                                            binding.stickName.animate().alpha(0.95f).setDuration(50).start()
                                        } else {
                                            binding.stickContainer.animate().setStartDelay(0).setDuration(25).scaleX(1f).scaleY(1f).start()

                                            binding.stickIcon.animate().alpha(0.25f).setDuration(25).start()
                                            binding.stickName.animate().alpha(0.35f).setDuration(25).start()
                                        }
                                    } else if (event.action == KeyEvent.ACTION_UP) {
                                        binding.stickNext.callOnClick()
                                    }
                                }

                                is AxisGuestEvent -> {
                                    val coefficient = if (event.action == KeyEvent.ACTION_DOWN) if (guestEvent.polarity) 1 else -1 else 0

                                    binding.stickContainer.apply {
                                        if (guestEvent.axis == item.stick.xAxis) {
                                            translationX = dipToPixels(16.5f) * coefficient
                                            rotationY = 27f * coefficient
                                        } else if (guestEvent.axis == item.stick.yAxis) {
                                            translationY = dipToPixels(16.5f) * -coefficient
                                            rotationX = 27f * coefficient
                                        }
                                    }
                                }

                                null -> if (event.action == KeyEvent.ACTION_UP) binding.stickNext.callOnClick()
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
                                    DialogStage.Button -> binding.stickContainer.animate().setStartDelay(0).setDuration(25).scaleX(0.85f).scaleY(0.85f).alpha(1f).start()

                                    DialogStage.YPlus, DialogStage.YMinus -> binding.stickContainer.animate().setStartDelay(0).setDuration(75).translationY(dipToPixels(16.5f) * coefficient).rotationX(27f * coefficient).alpha(1f).start()

                                    DialogStage.XPlus, DialogStage.XMinus -> binding.stickContainer.animate().setStartDelay(0).setDuration(75).translationX(dipToPixels(16.5f) * coefficient).rotationY(27f * coefficient).alpha(1f).start()
                                    else -> {
                                    }
                                }

                                binding.stickIcon.animate().alpha(0.85f).setDuration(50).start()
                                binding.stickName.animate().alpha(0.95f).setDuration(50).start()

                                binding.stickSubtitle.text = getString(R.string.release_confirm)
                                binding.stickSeekbar.visibility = View.GONE
                            } else if (deviceId == event.deviceId && inputId == event.keyCode && event.action == KeyEvent.ACTION_UP) {
                                // We serialize the current [deviceId] and [inputId] into a [KeyHostEvent] and map it to a corresponding [GuestEvent] and add it to [ignoredEvents] on [KeyEvent.ACTION_UP]
                                val hostEvent = KeyHostEvent(event.device.descriptor, event.keyCode)

                                var guestEvent = inputManager.eventMap[hostEvent]

                                if (guestEvent is GuestEvent) {
                                    inputManager.eventMap.remove(hostEvent)

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

                                inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                                inputManager.eventMap[hostEvent] = guestEvent

                                ignoredEvents.add(Objects.hash(deviceId!!, inputId!!))

                                item.update()

                                binding.stickNext.callOnClick()
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

            view.setOnGenericMotionListener { _, event ->
                // We retrieve the value of the HAT axes so that we can check for change and ignore any input from them so it'll be passed onto the [KeyEvent] handler
                val hat = Pair(event.getAxisValue(MotionEvent.AXIS_HAT_X), event.getAxisValue(MotionEvent.AXIS_HAT_Y))

                // We want all input events from Joysticks and Buttons that are [MotionEvent.ACTION_MOVE] and not from the D-pad
                if ((event.isFromSource(InputDevice.SOURCE_CLASS_JOYSTICK) || event.isFromSource(InputDevice.SOURCE_CLASS_BUTTON)) && event.action == MotionEvent.ACTION_MOVE && hat == oldHat) {
                    if (stage == DialogStage.Stick) {
                        // When the stick is being previewed after everything is mapped we do a lookup into [inputManager.eventMap] to find a corresponding [GuestEvent] and animate the stick correspondingly
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
                                inputManager.eventMap[hostEvent] ?: if (value == 0f) {
                                    polarity = false
                                    inputManager.eventMap[hostEvent.copy(polarity = false)]
                                } else {
                                    null
                                }
                            }

                            when (guestEvent) {
                                is ButtonGuestEvent -> {
                                    if (guestEvent.button == item.stick.button) {
                                        if (abs(value) >= guestEvent.threshold) {
                                            binding.stickContainer.animate().setStartDelay(0).setDuration(50).scaleX(0.85f).scaleY(0.85f).start()
                                            binding.stickIcon.animate().alpha(0.85f).setDuration(50).start()
                                            binding.stickName.animate().alpha(0.95f).setDuration(50).start()
                                        } else {
                                            binding.stickContainer.animate().setStartDelay(0).setDuration(25).scaleX(1f).scaleY(1f).start()
                                            binding.stickIcon.animate().alpha(0.25f).setDuration(25).start()
                                            binding.stickName.animate().alpha(0.35f).setDuration(25).start()
                                        }
                                    }
                                }

                                is AxisGuestEvent -> {
                                    value = guestEvent.value(value)

                                    val coefficient = if (polarity) abs(value) else -abs(value)

                                    binding.stickContainer.apply {
                                        if (guestEvent.axis == item.stick.xAxis) {
                                            translationX = dipToPixels(16.5f) * coefficient
                                            rotationY = 27f * coefficient
                                        } else if (guestEvent.axis == item.stick.yAxis) {
                                            translationY = dipToPixels(16.5f) * coefficient
                                            rotationX = 27f * -coefficient
                                        }
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

                                binding.stickSubtitle.text = getString(R.string.hold_confirm)

                                if (stage == DialogStage.Button)
                                    binding.stickSeekbar.visibility = View.VISIBLE

                                animationStop = true

                                break
                            }
                        }

                        // If the currently active input is a valid axis
                        if (axes.contains(inputId)) {
                            val value = event.getAxisValue(inputId!!)
                            val threshold = if (stage == DialogStage.Button) binding.stickSeekbar.progress / 100f else 0.5f

                            when (stage) {
                                // Update the secondary progress bar in [button_seekbar] based on the axis's value
                                DialogStage.Button -> {
                                    binding.stickContainer.animate().setStartDelay(0).setDuration(25).scaleX(0.85f).scaleY(0.85f).alpha(1f).start()
                                    binding.stickSeekbar.secondaryProgress = (abs(value) * 100).toInt()
                                }


                                // Update the the position of the stick in the Y-axis based on the axis's value
                                DialogStage.YPlus, DialogStage.YMinus -> {
                                    val coefficient = if (stage == DialogStage.YMinus) abs(value) else -abs(value)

                                    binding.stickContainer.translationY = dipToPixels(16.5f) * coefficient
                                    binding.stickContainer.rotationX = 27f * -coefficient
                                }

                                // Update the the position of the stick in the X-axis based on the axis's value
                                DialogStage.XPlus, DialogStage.XMinus -> {
                                    val coefficient = if (stage == DialogStage.XPlus) abs(value) else -abs(value)

                                    binding.stickContainer.translationX = dipToPixels(16.5f) * coefficient
                                    binding.stickContainer.rotationY = 27f * coefficient
                                }

                                else -> {
                                }
                            }

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

                                        val max = axesMax[axes.indexOf(inputId!!)]

                                        guestEvent = when (stage) {
                                            DialogStage.Button -> ButtonGuestEvent(controller.id, item.stick.button, threshold)
                                            DialogStage.YPlus, DialogStage.YMinus -> AxisGuestEvent(controller.id, item.stick.yAxis, stage == DialogStage.YPlus, max)
                                            DialogStage.XPlus, DialogStage.XMinus -> AxisGuestEvent(controller.id, item.stick.xAxis, stage == DialogStage.XPlus, max)
                                            else -> null
                                        }

                                        inputManager.eventMap.filterValues { it == guestEvent }.keys.forEach { inputManager.eventMap.remove(it) }

                                        inputManager.eventMap[hostEvent] = guestEvent

                                        ignoredEvents.add(Objects.hash(deviceId!!, inputId!!, axisPolarity))

                                        axisRunnable = null

                                        item.update()

                                        binding.stickNext.callOnClick()
                                    }

                                    handler.postDelayed(axisRunnable!!, 1000)
                                }

                                binding.stickIcon.animate().alpha(0.85f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                                binding.stickName.animate().alpha(0.95f).setInterpolator(LinearInterpolator(context, null)).setDuration(1000).start()
                            } else {
                                // If the axis value is below the threshold, remove [axisRunnable] from it being posted and animate the views accordingly
                                if (axisRunnable != null) {
                                    handler.removeCallbacks(axisRunnable!!)
                                    axisRunnable = null
                                }

                                if (stage == DialogStage.Button)
                                    binding.stickContainer.animate().setStartDelay(0).setDuration(10).scaleX(1f).scaleY(1f).alpha(1f).start()

                                binding.stickIcon.animate().alpha(0.25f).setDuration(50).start()
                                binding.stickName.animate().alpha(0.35f).setDuration(50).start()
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

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }
}
