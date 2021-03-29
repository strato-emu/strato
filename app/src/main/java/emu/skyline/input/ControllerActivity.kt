/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
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
import emu.skyline.utils.Settings
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

    @Inject
    lateinit var settings : Settings

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
                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_enable), oscSummary.invoke(settings.onScreenControl), settings.onScreenControl) { item, position ->
                    item.summary = oscSummary.invoke(item.checked)
                    settings.onScreenControl = item.checked
                    adapter.notifyItemChanged(position)
                })

                items.add(ControllerCheckBoxViewItem(getString(R.string.osc_recenter_sticks), "", settings.onScreenControlRecenterSticks) { item, position ->
                    settings.onScreenControlRecenterSticks = item.checked
                    adapter.notifyItemChanged(position)
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

            wroteTitle = false

            for (stick in controller.type.sticks) {
                if (!wroteTitle) {
                    items.add(ControllerHeaderItem(getString(R.string.sticks)))
                    wroteTitle = true
                }

                val stickItem = ControllerStickViewItem(id, stick, onControllerStickClick)

                items.add(stickItem)
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
                wroteTitle = false

                for (button in controller.type.buttons.filter { it in buttonArray.second }) {
                    if (!wroteTitle) {
                        items.add(ControllerHeaderItem(getString(buttonArray.first)))
                        wroteTitle = true
                    }

                    val buttonItem = ControllerButtonViewItem(id, button, onControllerButtonClick)

                    items.add(buttonItem)
                    buttonMap[button] = buttonItem
                }
            }

            wroteTitle = false

            for (button in controller.type.buttons.filterNot { item -> buttonArrays.any { item in it.second } }.plus(ButtonId.Menu)) {
                if (!wroteTitle) {
                    items.add(ControllerHeaderItem(getString(R.string.misc_buttons)))
                    wroteTitle = true
                }

                val buttonItem = ControllerButtonViewItem(id, button, onControllerButtonClick)

                items.add(buttonItem)
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

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        val layoutManager = LinearLayoutManager(this)
        binding.controllerList.layoutManager = layoutManager
        binding.controllerList.adapter = adapter

        binding.controllerList.addOnScrollListener(object : RecyclerView.OnScrollListener() {
            override fun onScrolled(recyclerView : RecyclerView, dx : Int, dy : Int) {
                super.onScrolled(recyclerView, dx, dy)

                if (layoutManager.findLastCompletelyVisibleItemPosition() == adapter.itemCount - 1) binding.titlebar.appBarLayout.setExpanded(false)
            }
        })

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
            onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }
}
