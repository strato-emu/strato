/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.*
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.preference.GpuDriverPreference
import emu.skyline.preference.IntegerListPreference
import emu.skyline.utils.GpuDriverHelper
import emu.skyline.utils.WindowInsetsHelper
import emu.skyline.utils.serializable

/**
 * This fragment is used to display custom game preferences
 */
class GameSettingsFragment : PreferenceFragmentCompat() {
    companion object {
        private const val DIALOG_FRAGMENT_TAG = "androidx.preference.PreferenceFragment.DIALOG"
    }

    private val item by lazy { requireArguments().serializable<AppItem>(AppItemTag)!! }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val recyclerView = view.findViewById<View>(R.id.recycler_view)
        WindowInsetsHelper.setPadding(recyclerView, bottom = true)

        (activity as AppCompatActivity).supportActionBar?.subtitle = item.title
    }

    /**
     * This constructs the preferences from XML preference resources
     */
    override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
        preferenceManager.sharedPreferencesName = EmulationSettings.prefNameForTitle(item.titleId ?: item.key())
        addPreferencesFromResource(R.xml.custom_game_preferences)
        addPreferencesFromResource(R.xml.emulation_preferences)

        // Toggle emulation settings enabled state based on use_custom_settings state
        listOf<Preference?>(
            findPreference("category_system"),
            findPreference("category_presentation"),
            findPreference("category_gpu"),
            findPreference("category_hacks"),
            findPreference("category_audio"),
            findPreference("category_debug")
        ).forEach { it?.dependency = "use_custom_settings" }

        // Uncheck `disable_frame_throttling` if `force_triple_buffering` gets disabled
        val disableFrameThrottlingPref = findPreference<CheckBoxPreference>("disable_frame_throttling")!!
        findPreference<CheckBoxPreference>("force_triple_buffering")?.setOnPreferenceChangeListener { _, newValue ->
            if (newValue == false)
                disableFrameThrottlingPref.isChecked = false
            true
        }

        // Only show validation layer setting in debug builds
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE != "release")
            findPreference<Preference>("validation_layer")?.isVisible = true

        if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
            val forceMaxGpuClocksPref = findPreference<CheckBoxPreference>("force_max_gpu_clocks")!!
            forceMaxGpuClocksPref.isSelectable = false
            forceMaxGpuClocksPref.isChecked = false
            forceMaxGpuClocksPref.summary = context!!.getString(R.string.force_max_gpu_clocks_desc_unsupported)
        }

        findPreference<GpuDriverPreference>("gpu_driver")?.item = item

        // Hide settings that don't support per-game configuration
        findPreference<Preference>("profile_picture_value")?.isVisible = false
        findPreference<Preference>("log_level")?.isVisible = false

        // TODO: remove this once we have more settings under the debug category
        // Avoid showing the debug category if no settings under it are visible
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE == "release")
            findPreference<PreferenceCategory>("category_debug")?.isVisible = false
    }

    override fun onDisplayPreferenceDialog(preference : Preference) {
        if (preference is IntegerListPreference) {
            // Check if dialog is already showing
            if (parentFragmentManager.findFragmentByTag(DIALOG_FRAGMENT_TAG) != null)
                return

            val dialogFragment = IntegerListPreference.IntegerListPreferenceDialogFragmentCompat.newInstance(preference.getKey())
            @Suppress("DEPRECATION")
            dialogFragment.setTargetFragment(this, 0) // androidx.preference.PreferenceDialogFragmentCompat depends on the target fragment being set correctly even though it's deprecated
            dialogFragment.show(parentFragmentManager, DIALOG_FRAGMENT_TAG)
        } else {
            super.onDisplayPreferenceDialog(preference)
        }
    }
}
