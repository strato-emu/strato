/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.text.InputFilter
import android.text.InputFilter.LengthFilter
import android.util.AttributeSet
import androidx.preference.EditTextPreference
import emu.skyline.R

/**
 * This class adapts [EditTextPreference] so that it supports setting the value as the summary automatically. Also added useful attributes.
 */
class CustomEditTextPreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = androidx.preference.R.attr.editTextPreferenceStyle) : EditTextPreference(context, attrs, defStyleAttr) {
    init {
        attrs?.let {
            val a = context.obtainStyledAttributes(it, R.styleable.CustomEditTextPreference, defStyleAttr, 0)
            val limit = a.getInt(R.styleable.CustomEditTextPreference_limit, -1)
            a.recycle()

            if (limit >= 0) {
                setOnBindEditTextListener { editText -> editText.filters = arrayOf<InputFilter>(LengthFilter(limit)) }
            }
        }

        setOnPreferenceChangeListener { _, newValue ->
            summary = newValue.toString()
            true
        }
    }

    override fun onAttached() {
        super.onAttached()

        summary = text
    }
}