/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.input

import android.os.Bundle
import android.view.KeyEvent
import android.view.View
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
class ControllerActivity : AppCompatActivity(), View.OnClickListener {
    /**
     * The index of the controller this activity manages
     */
    var id : Int = -1

    /**
     * The adapter used by [controller_list] to hold all the items
     */
    val adapter = ControllerAdapter(this)

    /**
     * The [InputManager] class handles loading/saving the input data
     */
    lateinit var manager : InputManager

    /**
     * This is a map between a button and it's corresponding [ControllerItem] in [adapter]
     */
    val buttonMap = mutableMapOf<ButtonId, ControllerItem>()

    /**
     * This is a map between an axis and it's corresponding [ControllerStickItem] in [adapter]
     */
    val axisMap = mutableMapOf<AxisId, ControllerStickItem>()

    /**
     * This function updates the [adapter] based on information from [manager]
     */
    private fun update() {
        adapter.clear()

        val controller = manager.controllers[id]!!

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

        manager = InputManager(this)

        id = intent.getIntExtra("index", 0)

        if (id < 0 || id > 7)
            throw IllegalArgumentException()

        title = "${getString(R.string.config_controller)} #${id + 1}"

        setContentView(R.layout.controller_activity)

        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        window.decorView.findViewById<View>(android.R.id.content).viewTreeObserver.addOnTouchModeChangeListener {
            if (!it)
                toolbar_layout.setExpanded(false)
        }

        controller_list.layoutManager = LinearLayoutManager(this)
        controller_list.adapter = adapter

        update()
    }

    /**
     * This causes the input file to be synced when the activity has been paused
     */
    override fun onPause() {
        manager.syncFile()
        super.onPause()
    }

    /**
     * This handles the onClick events for the items in the activity
     */
    override fun onClick(v : View?) {
        when (val tag = v!!.tag) {
            is ControllerTypeItem -> {
                val type = manager.controllers[id]!!.type

                val types = ControllerType.values().filter { !it.firstController || id == 0 }
                val typeNames = types.map { getString(it.stringRes) }.toTypedArray()

                MaterialAlertDialogBuilder(this)
                        .setTitle(tag.content)
                        .setSingleChoiceItems(typeNames, types.indexOf(type)) { dialog, typeIndex ->
                            manager.controllers[id] = when (types[typeIndex]) {
                                ControllerType.None -> Controller(id, ControllerType.None)
                                ControllerType.HandheldProController -> HandheldController(id)
                                ControllerType.ProController -> ProController(id)
                                ControllerType.JoyConLeft -> JoyConLeftController(id)
                                ControllerType.JoyConRight -> JoyConRightController(id)
                            }

                            update()

                            dialog.dismiss()
                        }
                        .show()
            }

            is ControllerGeneralItem -> {
                when (tag.type) {
                    GeneralType.PartnerJoyCon -> {
                        val controller = manager.controllers[id] as JoyConLeftController

                        val rJoyCons = manager.controllers.values.filter { it.type == ControllerType.JoyConRight }
                        val rJoyConNames = (listOf(getString(R.string.none)) + rJoyCons.map { "${getString(R.string.controller)} #${it.id + 1}" }).toTypedArray()

                        val partnerNameIndex = if (controller.partnerId == null) 0 else rJoyCons.withIndex().single { it.value.id == controller.partnerId }.index + 1

                        MaterialAlertDialogBuilder(this)
                                .setTitle(tag.content)
                                .setSingleChoiceItems(rJoyConNames, partnerNameIndex) { dialog, index ->
                                    (manager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = null

                                    controller.partnerId = if (index == 0) null else rJoyCons[index - 1].id

                                    if (controller.partnerId != null)
                                        (manager.controllers[controller.partnerId ?: -1] as JoyConRightController?)?.partnerId = controller.id

                                    tag.update()

                                    dialog.dismiss()
                                }
                                .show()
                    }

                    GeneralType.RumbleDevice -> {
                        val dialog = RumbleDialog(tag)
                        dialog.show(supportFragmentManager, null)
                    }
                }
            }

            is ControllerButtonItem -> {
                val dialog = ButtonDialog(tag)
                dialog.show(supportFragmentManager, null)
            }

            is ControllerStickItem -> {
                val dialog = StickDialog(tag)
                dialog.show(supportFragmentManager, null)
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
