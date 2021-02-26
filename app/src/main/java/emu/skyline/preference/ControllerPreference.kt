/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import emu.skyline.R
import emu.skyline.di.getInputManager
import emu.skyline.input.ControllerActivity

/**
 * This preference is used to launch [ControllerActivity] using a preference
 */
class ControllerPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val controllerCallback = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        inputManager.syncObjects()
        notifyChanged()
    }

    /**
     * The index of the controller this preference manages
     */
    private var index = -1

    private val inputManager = context.getInputManager()

    init {
        for (i in 0 until attrs!!.attributeCount) {
            val attr = attrs.getAttributeName(i)

            if (attr.equals("index", ignoreCase = true)) {
                index = attrs.getAttributeValue(i).toInt()
                break
            }
        }

        if (index == -1)
            throw IllegalArgumentException()

        if (key == null)
            key = "controller_$index"

        title = "${context.getString(R.string.config_controller)} #${index + 1}"
        summaryProvider = SummaryProvider<ControllerPreference> { inputManager.controllers[index]!!.type.stringRes.let { context.getString(it) } }
    }

    /**
     * This launches [ControllerActivity] on click to configure the controller
     */
    override fun onClick() = controllerCallback.launch(Intent(context, ControllerActivity::class.java))
}
