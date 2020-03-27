/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.Intent
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import kotlinx.android.synthetic.main.log_activity.*

class SettingsActivity : AppCompatActivity() {
    private val preferenceFragment: PreferenceFragment = PreferenceFragment()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.settings_activity)
        supportFragmentManager
                .beginTransaction()
                .replace(R.id.settings, preferenceFragment)
                .commit()
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
    }

    public override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        preferenceFragment.refreshPreferences()
    }

    class PreferenceFragment : PreferenceFragmentCompat() {
        fun refreshPreferences() {
            preferenceScreen = null
            addPreferencesFromResource(R.xml.preferences)
        }

        override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
            setPreferencesFromResource(R.xml.preferences, rootKey)
        }
    }
}
