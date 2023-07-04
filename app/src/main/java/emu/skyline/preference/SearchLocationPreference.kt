/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.content.Intent
import android.util.AttributeSet
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts
import androidx.preference.Preference
import androidx.preference.Preference.SummaryProvider
import androidx.preference.R
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.utils.SearchLocationHelper
import emu.skyline.R as SkylineR

/**
 * This preference is used to launch [SearchLocationActivity] using a preference
 */
class SearchLocationPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.preferenceStyle) : Preference(context, attrs, defStyleAttr) {
    private val searchLocationsCallback = (context as ComponentActivity).registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        setSummary()
        notifyChanged()
    }

    /**
     * The app item being configured, used to load the correct settings in [SearchLocationActivity]
     * This is populated by [emu.skyline.settings.GameSettingsFragment]
     */
    var item : AppItem? = null

    init { setSummary() }

    /**
     * This launches [SearchLocationActivity] on click to manage search locations
     */
    override fun onClick() = searchLocationsCallback.launch(Intent(context, SearchLocationActivity::class.java).apply {
        putExtra(AppItemTag, item)
    })

    /**
     * Sets the correct summary description to the app item
     */
    private fun setSummary() {
        val locations = SearchLocationHelper.getSearchLocations(context);
        summaryProvider = SummaryProvider<SearchLocationPreference> {
            String.format(
                context.getString(SkylineR.string.search_locations_count),
                if(locations.isEmpty()) "No" else locations.size.toString(),
                if(locations.size > 1) "s" else ""
            )
        }
    }
}
