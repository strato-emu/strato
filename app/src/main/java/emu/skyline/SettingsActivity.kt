/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.os.Bundle
import android.view.KeyEvent
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.PreferenceGroup
import emu.skyline.preference.ActivityResultDelegate
import emu.skyline.preference.DocumentActivity
import kotlinx.android.synthetic.main.settings_activity.*
import kotlinx.android.synthetic.main.titlebar.*

class SettingsActivity : AppCompatActivity() {
    /**
     * This is the instance of [PreferenceFragment] that is shown inside [R.id.settings]
     */
    private val preferenceFragment = PreferenceFragment()

    /**
     * This initializes all of the elements in the activity and displays the settings fragment
     */
    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.settings_activity)

        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)

        supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, preferenceFragment)
                .commit()
    }

    /**
     * This is used to refresh the preferences after [DocumentActivity] or [emu.skyline.input.ControllerActivity] has returned
     */
    public override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        super.onActivityResult(requestCode, resultCode, data)

        preferenceFragment.delegateActivityResult(requestCode, resultCode, data)

        settings
    }

    /**
     * This fragment is used to display all of the preferences
     */
    class PreferenceFragment : PreferenceFragmentCompat() {
        private var requestCodeCounter = 0

        /**
         * Delegates activity result to all preferences which implement [ActivityResultDelegate]
         */
        fun delegateActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
            preferenceScreen.delegateActivityResult(requestCode, resultCode, data)
        }

        /**
         * This constructs the preferences from [R.xml.preferences]
         */
        override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)
            preferenceScreen.assignActivityRequestCode()
        }

        private fun PreferenceGroup.assignActivityRequestCode() {
            for (i in 0 until preferenceCount) {
                when (val pref = getPreference(i)) {
                    is PreferenceGroup -> pref.assignActivityRequestCode()
                    is ActivityResultDelegate -> pref.requestCode = requestCodeCounter++
                }
            }
        }

        private fun PreferenceGroup.delegateActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
            for (i in 0 until preferenceCount) {
                when (val pref = getPreference(i)) {
                    is PreferenceGroup -> pref.delegateActivityResult(requestCode, resultCode, data)
                    is ActivityResultDelegate -> pref.onActivityResult(requestCode, resultCode, data)
                }
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
}
