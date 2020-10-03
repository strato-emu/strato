/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.os.Bundle
import android.view.KeyEvent
import androidx.appcompat.app.AppCompatActivity
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import emu.skyline.R
import emu.skyline.adapter.*
import emu.skyline.input.dialog.ButtonDialog
import emu.skyline.input.dialog.RumbleDialog
import emu.skyline.input.dialog.StickDialog
import kotlinx.android.synthetic.main.controller_activity.*
import kotlinx.android.synthetic.main.titlebar.*

/**
 * This activity is used to change the settings for a specific controller
 */
class ControllerActivity : AppCompatActivity() {
    /**
     * The index of the controller this activity manages
     */
    var id : Int = -1

    /**
     * The adapter used by [controller_list] to hold all the items
     */
    private val adapter = ControllerAdapter(::onControllerItemClick)

    /**
     * This is a map between a button and it's corresponding [ControllerItem] in [adapter]
     */
    val buttonMap = mutableMapOf<ButtonId, ControllerItem>()

    /**
     * This is a map between an axis and it's corresponding [ControllerStickItem] in [adapter]
     */
    val axisMap = mutableMapOf<AxisId, ControllerStickItem>()

    /**
     * This function updates the [adapter] based on information from [InputManager]
     */
    private fun update() {
        adapter.clear()

        val controller = InputManager.controllers[id]!!

        adapter.addItem(ControllerTypeItem(this, controller.type))

        if (controller.type == ControllerType.None)
            return

        var wroteTitle = false

        for (item in GeneralType.values()) {
            if (item.compatibleControllers == null || item.compatibleControllers.contains(controller.type)) {
                if (!wroteTitle) {
                    adapter.addHeader(getString(R.string.general))
                    wroteTitle = true
                }

                adapter.addItem(ControllerGeneralItem(this, item))
            }
        }

        wroteTitle = false

        for (stick in controller.type.sticks) {
            if (!wroteTitle) {
                adapter.addHeader(getString(R.string.sticks))
                wroteTitle = true
            }

            val stickItem = ControllerStickItem(this, stick)

            adapter.addItem(stickItem)
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
                    adapter.addHeader(getString(buttonArray.first))
                    wroteTitle = true
                }

                val buttonItem = ControllerButtonItem(this, button)

                adapter.addItem(buttonItem)
                buttonMap[button] = buttonItem
            }
        }

        wroteTitle = false

        for (button in controller.type.buttons.filterNot { item -> buttonArrays.any { item in it.second } }.plus(ButtonId.Menu)) {
            if (!wroteTitle) {
                adapter.addHeader(getString(R.string.misc_buttons))
                wroteTitle = true
            }

            val buttonItem = ControllerButtonItem(this, button)

            adapter.addItem(buttonItem)
            buttonMap[button] = buttonItem
        }
    }

    /**
     * This initializes all of the elements in the activity
     */
    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        id = intent.getIntExtra("index", 0)

        if (id < 0 || id > 7)
            throw IllegalArgumentException()

        title = "${getString(R.string.config_controller)} #${id + 1}"

        setContentView(R.layout.controller_activity)

        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        controller_list.layoutManager = LinearLayoutManager(this)
        controller_list.adapter = adapter

        update()
    }

    /**
     * This causes the input file to be synced when the activity has been paused
     */
    override fun onPause() {
        InputManager.syncFile()
        super.onPause()
    }

    private fun onControllerItemClick(item : ControllerItem) {
        when (item) {
            is ControllerTypeItem -> {
                val controller = InputManager.controllers[id]!!

                val types = ControllerType.values().filter { !it.firstController || id == 0 }
                val typeNames = types.map { getString(it.stringRes) }.toTypedArray()

                MaterialAlertDialogBuilder(this)
                        .setTitle(item.content)
                        .setSingleChoiceItems(typeNames, types.indexOf(controller.type)) { dialog, typeIndex ->
                            val selectedType = types[typeIndex]
                            if (controller.type != selectedType) {
                                if (controller is JoyConLeftController)
                                    controller.partnerId?.let { (InputManager.controllers[it] as JoyConRightController).partnerId = null }
                                else if (controller is JoyConRightController)
                                    controller.partnerId?.let { (InputManager.controllers[it] as JoyConLeftController).partnerId = null }

                                InputManager.controllers[id] = when (selectedType) {
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
            }

            is ControllerGeneralItem -> {
                when (item.type) {
                    GeneralType.PartnerJoyCon -> {
                        val controller = InputManager.controllers[id] as JoyConLeftController

                        val rJoyCons = InputManager.controllers.values.filter { it.type == ControllerType.JoyConRight }
                        val rJoyConNames = (listOf(getString(R.string.none)) + rJoyCons.map { "${getString(R.string.controller)} #${it.id + 1}" }).toTypedArray()

                        val partnerNameIndex = controller.partnerId?.let { partnerId ->
                            rJoyCons.withIndex().single { it.value.id == partnerId }.index + 1
                        } ?: 0

                        MaterialAlertDialogBuilder(this)
                                .setTitle(item.content)
                                .setSingleChoiceItems(rJoyConNames, partnerNameIndex) { dialog, index ->
                                    (InputManager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = null

                                    controller.partnerId = if (index == 0) null else rJoyCons[index - 1].id

                                    if (controller.partnerId != null)
                                        (InputManager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = controller.id

                                    item.update()

                                    dialog.dismiss()
                                }
                                .show()
                    }

                    GeneralType.RumbleDevice -> {
                        RumbleDialog(item).show(supportFragmentManager, null)
                    }
                }
            }

            is ControllerButtonItem -> {
                ButtonDialog(item).show(supportFragmentManager, null)
            }

            is ControllerStickItem -> {
                StickDialog(item).show(supportFragmentManager, null)
            }
        }
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
