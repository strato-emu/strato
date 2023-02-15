/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.settings

import android.os.Bundle
import android.view.*
import androidx.appcompat.app.AppCompatActivity
import androidx.coordinatorlayout.widget.CoordinatorLayout
import androidx.core.view.*
import androidx.preference.*
import emu.skyline.BuildConfig
import emu.skyline.R
import emu.skyline.databinding.SettingsActivityBinding
import emu.skyline.preference.IntegerListPreference
import emu.skyline.utils.GpuDriverHelper
import emu.skyline.utils.WindowInsetsHelper

class SettingsActivity : AppCompatActivity() {
    val binding by lazy { SettingsActivityBinding.inflate(layoutInflater) }

    /**
     * This is the instance of [PreferenceFragment] that is shown inside [R.id.settings]
     */
    private val preferenceFragment = PreferenceFragment()

    /**
     * This initializes all of the elements in the activity and displays the settings fragment
     */
    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(binding.root)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsHelper.applyToActivity(binding.root)

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        var layoutDone = false // Tracks if the layout is complete to avoid retrieving invalid attributes
        binding.coordinatorLayout.viewTreeObserver.addOnTouchModeChangeListener { isTouchMode ->
            val layoutUpdate = {
                val params = binding.settings.layoutParams as CoordinatorLayout.LayoutParams
                if (!isTouchMode) {
                    binding.titlebar.appBarLayout.setExpanded(true)
                    params.height = binding.coordinatorLayout.height - binding.titlebar.toolbar.height
                } else {
                    params.height = CoordinatorLayout.LayoutParams.MATCH_PARENT
                }

                binding.settings.layoutParams = params
                binding.settings.requestLayout()
            }

            if (!layoutDone) {
                binding.coordinatorLayout.viewTreeObserver.addOnGlobalLayoutListener(object : ViewTreeObserver.OnGlobalLayoutListener {
                    override fun onGlobalLayout() {
                        // We need to wait till the layout is done to get the correct height of the toolbar
                        binding.coordinatorLayout.viewTreeObserver.removeOnGlobalLayoutListener(this)
                        layoutUpdate()
                        layoutDone = true
                    }
                })
            } else {
                layoutUpdate()
            }
        }

        supportFragmentManager
            .beginTransaction()
            .replace(R.id.settings, preferenceFragment)
            .commit()
    }

    /**
     * This fragment is used to display all of the preferences
     */
    class PreferenceFragment : PreferenceFragmentCompat() {
        companion object {
            private const val DIALOG_FRAGMENT_TAG = "androidx.preference.PreferenceFragment.DIALOG"
        }

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
            addPreferencesFromResource(R.xml.game_preferences)
            addPreferencesFromResource(R.xml.input_preferences)
            addPreferencesFromResource(R.xml.credits_preferences)

            // Uncheck `disable_frame_throttling` if `force_triple_buffering` gets disabled
            val disableFrameThrottlingPref = findPreference<CheckBoxPreference>("disable_frame_throttling")!!
            findPreference<CheckBoxPreference>("force_triple_buffering")?.setOnPreferenceChangeListener { _, newValue ->
                if (newValue == false)
                    disableFrameThrottlingPref.isChecked = false
                true
            }

            // Only show debug settings in debug builds
            @Suppress("SENSELESS_COMPARISON")
            if (BuildConfig.BUILD_TYPE != "release")
                findPreference<Preference>("category_debug")?.isVisible = true


            if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
                val forceMaxGpuClocksPref = findPreference<CheckBoxPreference>("force_max_gpu_clocks")!!
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

    /**
     * This handles on calling [onBackPressed] when [KeyEvent.KEYCODE_BUTTON_B] is lifted
     */
    override fun onKeyUp(keyCode : Int, event : KeyEvent?) : Boolean {
        if (keyCode == KeyEvent.KEYCODE_BUTTON_B) {
            onBackPressedDispatcher.onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }

    override fun finish() {
        setResult(RESULT_OK)
        super.finish()
    }
}
