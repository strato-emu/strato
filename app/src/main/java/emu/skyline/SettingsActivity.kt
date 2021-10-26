/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import emu.skyline.databinding.SettingsActivityBinding
import emu.skyline.preference.IntegerListPreference

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

        setSupportActionBar(binding.titlebar.toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        window.decorView.findViewById<View>(android.R.id.content).viewTreeObserver.addOnTouchModeChangeListener { isInTouchMode ->
            if (!isInTouchMode) binding.titlebar.appBarLayout.setExpanded(false)
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

        /**
         * This constructs the preferences from [R.xml.preferences]
         */
        override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)
        }

        override fun onDisplayPreferenceDialog(preference : Preference?) {
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
            onBackPressed()
            return true
        }

        return super.onKeyUp(keyCode, event)
    }

    override fun finish() {
        setResult(RESULT_OK)
        super.finish()
    }
}
