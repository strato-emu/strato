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
import kotlinx.android.synthetic.main.titlebar.*

class SettingsActivity : AppCompatActivity() {
    /**
     * This is the instance of [PreferenceFragment] that is shown inside [R.id.settings]
     */
    private val preferenceFragment : PreferenceFragment = PreferenceFragment()

    /**
     * This initializes [toolbar] and [R.id.settings]
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
     * This is used to refresh the preferences after [emu.skyline.preference.FolderActivity] has returned
     */
    public override fun onActivityResult(requestCode : Int, resultCode : Int, data : Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        preferenceFragment.refreshPreferences()
    }

    /**
     * This fragment is used to display all of the preferences and handle refreshing the preferences
     */
    class PreferenceFragment : PreferenceFragmentCompat() {
        /**
         * This clears the preference screen and reloads all preferences
         */
        fun refreshPreferences() {
            preferenceScreen = null
            addPreferencesFromResource(R.xml.preferences)
        }

        /**
         * This constructs the preferences from [R.xml.preferences]
         */
        override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)
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
