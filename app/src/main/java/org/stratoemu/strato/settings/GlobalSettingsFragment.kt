/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.settings

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import androidx.preference.TwoStatePreference
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import org.stratoemu.strato.BuildConfig
import org.stratoemu.strato.MainActivity
import org.stratoemu.strato.R
import org.stratoemu.strato.utils.GpuDriverHelper
import org.stratoemu.strato.utils.WindowInsetsHelper
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * This fragment is used to display the global preferences
 */
class GlobalSettingsFragment : PreferenceFragmentCompat() {
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val recyclerView = view.findViewById<View>(R.id.recycler_view)
        WindowInsetsHelper.setPadding(recyclerView, bottom = true)
    }

    /**
     * This constructs the preferences from XML preference resources
     */
    override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
        addPreferencesFromResource(R.xml.app_preferences)
        addPreferencesFromResource(R.xml.emulation_preferences)
        addPreferencesFromResource(R.xml.input_preferences)
        addPreferencesFromResource(R.xml.credits_preferences)

        // Re-launch the app if Material You is toggled
        findPreference<Preference>("use_material_you")?.setOnPreferenceChangeListener { _, _ ->
            requireActivity().finishAffinity()
            startActivity(Intent(requireContext(), MainActivity::class.java))
            true
        }

        CoroutineScope(Dispatchers.IO).launch {
            WindowInfoTracker.getOrCreate(requireContext()).windowLayoutInfo(requireActivity()).collect { newLayoutInfo ->
                withContext(Dispatchers.Main) {
                    findPreference<SwitchPreferenceCompat>("enable_foldable_layout")?.isVisible = newLayoutInfo.displayFeatures.find { it is FoldingFeature } != null
                }
            }
        }

        // Uncheck `disable_frame_throttling` if `force_triple_buffering` gets disabled
        val disableFrameThrottlingPref = findPreference<TwoStatePreference>("disable_frame_throttling")!!
        findPreference<TwoStatePreference>("force_triple_buffering")?.setOnPreferenceChangeListener { _, newValue ->
            if (newValue == false)
                disableFrameThrottlingPref.isChecked = false
            true
        }

        // Only show validation layer setting in debug builds
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE != "release")
            findPreference<Preference>("validation_layer")?.isVisible = true

        if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
            val forceMaxGpuClocksPref = findPreference<TwoStatePreference>("force_max_gpu_clocks")!!
            forceMaxGpuClocksPref.isSelectable = false
            forceMaxGpuClocksPref.isChecked = false
            forceMaxGpuClocksPref.summary = context!!.getString(R.string.force_max_gpu_clocks_desc_unsupported)
        }

        resources.getStringArray(R.array.credits_entries).asIterable().shuffled().forEach {
            findPreference<PreferenceCategory>("category_credits")?.addPreference(Preference(context!!).apply {
                title = it
            })
        }
    }
}
