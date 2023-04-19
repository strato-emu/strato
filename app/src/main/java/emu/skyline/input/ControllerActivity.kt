/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.content.Intent
import android.graphics.Canvas
import android.os.Bundle
import android.view.KeyEvent
import android.view.ViewTreeObserver
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.WindowCompat
import androidx.core.view.marginTop
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import dagger.hilt.android.AndroidEntryPoint
import emu.skyline.R
import emu.skyline.adapter.GenericAdapter
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.controller.*
import emu.skyline.databinding.ControllerActivityBinding
import emu.skyline.input.dialog.ButtonDialog
import emu.skyline.input.dialog.RumbleDialog
import emu.skyline.input.dialog.StickDialog
import emu.skyline.input.onscreen.OnScreenEditActivity
import emu.skyline.settings.AppSettings
import emu.skyline.utils.WindowInsetsHelper
import javax.inject.Inject

/**
 * This activity is used to change the settings for a specific controller
 */
@AndroidEntryPoint
class ControllerActivity : AppCompatActivity() {
    private val binding by lazy { ControllerActivityBinding.inflate(layoutInflater) }

    /**
     * The index of the controller this activity manages
     */
    val id by lazy { intent.getIntExtra("index", 0) }

    private val adapter = GenericAdapter()

    /**
     * This is a map between a button and it's corresponding [ControllerViewItem] in [adapter]
     */
    val buttonMap = mutableMapOf<ButtonId, ControllerViewItem>()

    /**
     * This is a map between an axis and it's corresponding [ControllerStickViewItem] in [adapter]
     */
    val axisMap = mutableMapOf<AxisId, ControllerStickViewItem>()

    @Suppress("WeakerAccess")
    val stickItems = mutableListOf<ControllerStickViewItem>()

    @Suppress("WeakerAccess")
    val buttonItems = mutableListOf<ControllerButtonViewItem>()

    @Inject
    lateinit var appSettings : AppSettings

    @Inject
    lateinit var inputManager : InputManager

    /**
     * This function updates the [adapter] based on information from [InputManager]
     */
    private fun update() {
        val items = mutableListOf<GenericListItem<*>>()

        try {
            val controller = inputManager.controllers[id]!!

            items.add(ControllerTypeViewItem(controller.type, onControllerTypeClick))

            if (controller.type == ControllerType.None)
                return

            if (id == 0) {
                items.add(ControllerHeaderItem(getString(R.string.osc)))

                val oscSummary = { checked : Boolean -> getString(if (checked) R.string.osc_shown else R.string.osc_not_shown) }
                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_enable), oscSummary.invoke(appSettings.onScreenControl), appSettings.onScreenControl) { item, position ->
                    item.summary = oscSummary.invoke(item.checked)
                    appSettings.onScreenControl = item.checked
                })

                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_feedback), getString(R.string.osc_feedback_description), appSettings.onScreenControlFeedback) { item, position ->
                    appSettings.onScreenControlFeedback = item.checked
                })

                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_recenter_sticks), getString(R.string.osc_recenter_sticks_desc), appSettings.onScreenControlRecenterSticks) { item, position ->
                    appSettings.onScreenControlRecenterSticks = item.checked
                })

                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_use_stick_regions), getString(R.string.osc_use_stick_regions_desc), appSettings.onScreenControlUseStickRegions) { item, position ->
                    appSettings.onScreenControlUseStickRegions = item.checked
                })

                items.add(ControllerViewItem(content = getString(R.string.osc_edit), onClick = {
                    startActivity(Intent(this, OnScreenEditActivity::class.java))
                }))
            }

            var wroteTitle = false

            for (item in GeneralType.values()) {
                if (item.compatibleControllers == null || item.compatibleControllers.contains(controller.type)) {
                    if (!wroteTitle) {
                        items.add(ControllerHeaderItem(getString(R.string.general)))
                        wroteTitle = true
                    }

                    items.add(ControllerGeneralViewItem(id, item, onControllerGeneralClick))
                }
            }

            if (controller.type.sticks.isNotEmpty())
                items.add(ControllerHeaderItem(getString(R.string.sticks)))

            for (stick in controller.type.sticks) {
                val stickItem = ControllerStickViewItem(id, stick, onControllerStickClick)

                items.add(stickItem)
                stickItems.add(stickItem)
                buttonMap[stick.button] = stickItem
                axisMap[stick.xAxis] = stickItem
                axisMap[stick.yAxis] = stickItem
            }

            val dpadButtons = Pair(R.string.dpad, arrayOf(ButtonId.DpadUp, ButtonId.DpadDown, ButtonId.DpadLeft, ButtonId.DpadRight))
            val faceButtons = Pair(R.string.face_buttons, arrayOf(ButtonId.A, ButtonId.B, ButtonId.X, ButtonId.Y))
            val shoulderTriggerButtons = Pair(R.string.shoulder_trigger, arrayOf(ButtonId.L, ButtonId.R, ButtonId.ZL, ButtonId.ZR))
            val shoulderRailButtons = Pair(R.string.shoulder_rail, arrayOf(ButtonId.LeftSL, ButtonId.LeftSR, ButtonId.RightSL, ButtonId.RightSR))

            val buttonArrays = arrayOf(dpadButtons, faceButtons, shoulderTriggerButtons, shoulderRailButtons)

            for (buttonArray in buttonArrays) {
                val filteredButtons = controller.type.buttons.filter { it in buttonArray.second }

                if (filteredButtons.isNotEmpty())
                    items.add(ControllerHeaderItem(getString(buttonArray.first)))

                for (button in filteredButtons) {
                    val buttonItem = ControllerButtonViewItem(id, button, onControllerButtonClick)

                    items.add(buttonItem)
                    buttonItems.add(buttonItem)
                    buttonMap[button] = buttonItem
                }
            }

            items.add(ControllerHeaderItem(getString(R.string.misc_buttons))) // The menu button will always exist
            for (button in controller.type.buttons.filterNot { item -> buttonArrays.any { item in it.second } }.plus(ButtonId.Menu)) {
                val buttonItem = ControllerButtonViewItem(id, button, onControllerButtonClick)

                items.add(buttonItem)
                buttonItems.add(buttonItem)
                buttonMap[button] = buttonItem
            }
        } finally {
            adapter.setItems(items)
        }
    }

    /**
     * This initializes all of the elements in the activity
     */
    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        if (id < 0 || id > 7)
            throw IllegalArgumentException()

        title = "${getString(R.string.config_controller)} #${id + 1}"

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root, binding.controllerList)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        val layoutManager = LinearLayoutManager(this)
        binding.controllerList.layoutManager = layoutManager
        binding.controllerList.adapter = adapter

        var layoutDone = false // Tracks if the layout is complete to avoid retrieving invalid attributes
        binding.coordinatorLayout.viewTreeObserver.addOnTouchModeChangeListener { isTouchMode ->
            val layoutUpdate = {
                val params = binding.controllerList.layoutParams as CoordinatorLayout.LayoutParams
                if (!isTouchMode) {
                    binding.titlebar.appBarLayout.setExpanded(true)
                    params.height = binding.coordinatorLayout.height - binding.titlebar.toolbar.height
                } else {
                    params.height = CoordinatorLayout.LayoutParams.MATCH_PARENT
                }

                binding.controllerList.layoutParams = params
                binding.controllerList.requestLayout()
            }

            if (!layoutDone) {
                binding.coordinatorLayout.viewTreeObserver.addOnGlobalLayoutListener(object : ViewTreeObserver.OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        // We need to wait till the layout is done to get the correct height of the toolbar
                        binding.coordinatorLayout.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        layoutUpdate()
                        layoutDone = true
                    }
                })
            } else {
                layoutUpdate()
            }
        }

        val dividerItemDecoration = object : DividerItemDecoration(this, VERTICAL) {
            override fun onDraw(canvas : Canvas, parent : RecyclerView, state : RecyclerView.State) {
                val divider = drawable!!
                for (i in 0 until parent.childCount) {
                    val view = parent.getChildAt(i)
                    val position = parent.getChildAdapterPosition(view)
                    if (position != RecyclerView.NO_POSITION && parent.adapter!!.getItemViewType(position) == adapter.getFactoryViewType(ControllerHeaderBindingFactory)) {
                        val bottom = view.top - view.marginTop
                        val top = bottom - divider.intrinsicHeight
                        divider.setBounds(0, top, parent.width, bottom)
                        divider.draw(canvas)
                    }
                }
            }
        }
        binding.controllerList.addItemDecoration(dividerItemDecoration)

        update()
    }

    /**
     * This causes the input file to be synced when the activity has been paused
     */
    override fun onPause() {
        inputManager.syncFile()
        super.onPause()
    }

    private val onControllerTypeClick = { item : ControllerTypeViewItem, _ : Int ->
        val controller = inputManager.controllers[id]!!

        val types = ControllerType.values().apply { if (id != 0) filter { !it.firstController } }
        val typeNames = types.map { getString(it.stringRes) }.toTypedArray()

        MaterialAlertDialogBuilder(this)
            .setTitle(item.content)
            .setSingleChoiceItems(typeNames, types.indexOf(controller.type)) { dialog, typeIndex ->
                val selectedType = types[typeIndex]
                if (controller.type != selectedType) {
                    if (controller is JoyConLeftController)
                        controller.partnerId?.let { (inputManager.controllers[it] as JoyConRightController).partnerId = null }
                    else if (controller is JoyConRightController)
                        controller.partnerId?.let { (inputManager.controllers[it] as JoyConLeftController).partnerId = null }

                    inputManager.controllers[id] = when (selectedType) {
                        ControllerType.None -> Controller(id, ControllerType.None)
                        ControllerType.HandheldProController -> HandheldController(id)
                        ControllerType.ProController -> ProController(id)
                        ControllerType.JoyConLeft -> JoyConLeftController(id)
                        ControllerType.JoyConRight -> JoyConRightController(id)
                    }

                    update()
                }

                dialog.dismiss()
            }
            .show()
        Unit
    }

    private val onControllerGeneralClick = { item : ControllerGeneralViewItem, _ : Int ->
        when (item.type) {
            GeneralType.PartnerJoyCon -> {
                val controller = inputManager.controllers[id] as JoyConLeftController

                val rJoyCons = inputManager.controllers.values.filter { it.type == ControllerType.JoyConRight }
                val rJoyConNames = (listOf(getString(R.string.none)) + rJoyCons.map { "${getString(R.string.controller)} #${it.id + 1}" }).toTypedArray()

                val partnerNameIndex = controller.partnerId?.let { partnerId ->
                    rJoyCons.withIndex().single { it.value.id == partnerId }.index + 1
                } ?: 0

                MaterialAlertDialogBuilder(this)
                    .setTitle(item.content)
                    .setSingleChoiceItems(rJoyConNames, partnerNameIndex) { dialog, index ->
                        (inputManager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = null

                        controller.partnerId = if (index == 0) null else rJoyCons[index - 1].id

                        if (controller.partnerId != null)
                            (inputManager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = controller.id

                        item.update()

                        dialog.dismiss()
                    }
                    .show()
            }

            GeneralType.RumbleDevice -> {
                RumbleDialog(item).show(supportFragmentManager, null)
            }

            GeneralType.SetupGuide -> {
                var dialogFragment : BottomSheetDialogFragment? = null

                for (buttonItem in buttonItems.reversed().filter { it.button != ButtonId.Menu })
                    dialogFragment = ButtonDialog(buttonItem, dialogFragment)

                for (stickItem in stickItems.reversed())
                    dialogFragment = StickDialog(stickItem, dialogFragment)

                dialogFragment?.show(supportFragmentManager, null)
            }
        }
        Unit
    }

    private val onControllerButtonClick = { item : ControllerButtonViewItem, _ : Int ->
        ButtonDialog(item).show(supportFragmentManager, null)
    }

    private val onControllerStickClick = { item : ControllerStickViewItem, _ : Int ->
        StickDialog(item).show(supportFragmentManager, null)
    }

    /**
     * This handles on calling [onBackPressed] when [KeyEvent.KEYCODE_BUTTON_B] is lifted
     */
    override fun onKeyUp(keyCode : Int, event : KeyEvent?) : Boolean {
        if (keyCode == KeyEvent.KEYCODE_BUTTON_B) {
            onBackPressedDispatcher.onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }
}
