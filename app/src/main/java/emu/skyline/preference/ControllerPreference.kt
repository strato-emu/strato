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
import androidx.preference.R
import emu.skyline.di.getInputManager
import emu.skyline.input.ControllerActivity
import emu.skyline.R as SkylineR

/**
 * This preference is used to launch [ControllerActivity] using a preference
 */
class ControllerPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val controllerCallback = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        inputManager.syncObjects()
        notifyChanged()
    }

    companion object {
        const val INDEX_ARG = "index"
    }

    /**
     * The index of the controller this preference manages
     */
    private var index = -1

    private val inputManager = context.getInputManager()

    init {
        for (i in 0 until attrs!!.attributeCount) {
            val attr = attrs.getAttributeName(i)

            if (attr.equals(INDEX_ARG)) {
                index = attrs.getAttributeValue(i).toInt()
                break
            }
        }

        if (index == -1)
            throw IllegalArgumentException()

        if (key == null)
            key = "controller_$index"

        title = "${context.getString(SkylineR.string.config_controller)} #${index + 1}"
        summaryProvider = SummaryProvider<ControllerPreference> { inputManager.controllers[index]!!.type.stringRes.let { context.getString(it) } }
    }

    /**
     * This launches [ControllerActivity] on click to configure the controller
     */
    override fun onClick() = controllerCallback.launch(Intent(context, ControllerActivity::class.java).putExtra(INDEX_ARG, index))
}
