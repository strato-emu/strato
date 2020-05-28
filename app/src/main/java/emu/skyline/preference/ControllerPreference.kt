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

/**
 * This preference is used to launch [ControllerActivity] using a preference
 */
class ControllerPreference : Preference {
    /**
     * The index of the controller this preference manages
     */
    private var index : Int = -1

    constructor(context : Context?, attrs : AttributeSet?, defStyleAttr : Int) : super(context, attrs, defStyleAttr) {
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

        if (context is SettingsActivity)
            summaryProvider = SummaryProvider<ControllerPreference> { _ -> context.inputManager.controllers[index]?.type?.stringRes?.let { context.getString(it) } }
    }

    constructor(context : Context?, attrs : AttributeSet?) : this(context, attrs, R.attr.preferenceStyle)

    constructor(context : Context?) : this(context, null)

    /**
     * This launches [ControllerActivity] on click to configure the controller
     */
    override fun onClick() {
        if (context is SettingsActivity)
            (context as SettingsActivity).refreshKey = key

        val intent = Intent(context, ControllerActivity::class.java)
        intent.putExtra("index", index)
        (context as Activity).startActivityForResult(intent, 0)
    }
}
