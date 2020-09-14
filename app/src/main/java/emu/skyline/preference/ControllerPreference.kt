/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import emu.skyline.R
import emu.skyline.SettingsActivity
import emu.skyline.input.ControllerActivity
import emu.skyline.input.InputManager

/**
 * This preference is used to launch [ControllerActivity] using a preference
 */
class ControllerPreference @JvmOverloads constructor(context : Context?, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr), ActivityResultDelegate {
    /**
     * The index of the controller this preference manages
     */
    private var index = -1

    private var inputManager : InputManager? = null

    override var requestCode = 0

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

        title = "${context?.getString(R.string.config_controller)} #${index + 1}"

        if (context is SettingsActivity) {
            inputManager = context.inputManager
            summaryProvider = SummaryProvider<ControllerPreference> { context.inputManager.controllers[index]?.type?.stringRes?.let { context.getString(it) } }
        }
    }

    /**
     * This launches [ControllerActivity] on click to configure the controller
     */
    override fun onClick() {
        (context as Activity).startActivityForResult(Intent(context, ControllerActivity::class.java).apply { putExtra("index", index) }, requestCode)
    }

    override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        if (this.requestCode == requestCode) {
            inputManager?.syncObjects()
            notifyChanged()
        }
    }
}
