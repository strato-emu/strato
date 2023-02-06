/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.util.AttributeSet
import androidx.appcompat.app.AppCompatDelegate
import androidx.core.os.LocaleListCompat
import androidx.preference.ListPreference
import androidx.preference.Preference.OnPreferenceChangeListener
import androidx.preference.Preference.SummaryProvider
import emu.skyline.BuildConfig
import emu.skyline.R
import java.util.*

private const val SystemLanguage = "syslang"

class LanguagePreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.preferenceStyle) : ListPreference(context, attrs, defStyleAttr) {
    init {
        entries = arrayOf(systemEntry) + BuildConfig.AVAILABLE_APP_LANGUAGES.map { Locale.forLanguageTag(it).run { getDisplayName(this) } }
        entryValues = arrayOf(SystemLanguage) + BuildConfig.AVAILABLE_APP_LANGUAGES

        value = AppCompatDelegate.getApplicationLocales()[0]?.toLanguageTag() ?: SystemLanguage

        onPreferenceChangeListener = OnPreferenceChangeListener { _, newValue ->
            if (newValue == SystemLanguage) {
                AppCompatDelegate.setApplicationLocales(LocaleListCompat.getEmptyLocaleList())
            } else {
                val newLocale = LocaleListCompat.forLanguageTags(newValue as String)
                AppCompatDelegate.setApplicationLocales(newLocale)
            }
            true
        }

        summaryProvider = SummaryProvider<ListPreference> {
            AppCompatDelegate.getApplicationLocales()[0]?.displayName ?: systemEntry
        }
    }

    private val systemEntry : String get() = context.getString(R.string.app_language_default)
}
