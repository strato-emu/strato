/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.R as AndroidR
import org.stratoemu.strato.data.BaseAppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.settings.EmulationSettings
import org.stratoemu.strato.utils.GpuDriverHelper
import org.stratoemu.strato.R

/**
 * This preference is used to launch [GpuDriverActivity] using a preference
 */
class GpuDriverPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = AndroidR.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val driverCallback = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        notifyChanged()
    }

    /**
     * The app item being configured, used to load the correct settings in [GpuDriverActivity]
     * This is populated by [org.stratoemu.strato.settings.GameSettingsFragment]
     */
    var item : BaseAppItem? = null

    init {
        val supportsCustomDriverLoading = GpuDriverHelper.supportsCustomDriverLoading()
        if (supportsCustomDriverLoading) {
            summaryProvider = SummaryProvider<GpuDriverPreference> {
                sharedPreferences?.getString(key, EmulationSettings.SYSTEM_GPU_DRIVER)?.let {
                    var driver = it
                    if (it == EmulationSettings.SYSTEM_GPU_DRIVER)
                        driver = context.getString(R.string.system_driver)

                    context.getString(R.string.gpu_driver_config_desc, driver)
                }
            }
        } else {
            isEnabled = false
            summaryProvider = SummaryProvider<GpuDriverPreference> {
                context.getString(R.string.gpu_driver_config_desc_unsupported)
            }
        }
    }

    /**
     * This launches [GpuDriverActivity] on click to manage driver packages
     */
    override fun onClick() = driverCallback.launch(Intent(context, GpuDriverActivity::class.java).apply {
        putExtra(AppItemTag, item)
    })
}
