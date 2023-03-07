/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.annotation.SuppressLint
import android.content.Context
import android.content.DialogInterface
import android.content.res.Resources
import android.content.res.TypedArray
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.util.AttributeSet
import androidx.annotation.ArrayRes
import androidx.appcompat.app.AlertDialog
import androidx.core.content.res.TypedArrayUtils
import androidx.preference.DialogPreference
import androidx.preference.PreferenceDialogFragmentCompat
import androidx.preference.R
import emu.skyline.di.getSettings
import emu.skyline.R as sR

/**
 * A Preference that displays a list of strings in a dialog and saves an integer that corresponds to the selected entry (as specified by entryValues or the index of the selected entry)
 * @see androidx.preference.ListPreference
 */
@SuppressLint("RestrictedApi")
open class IntegerListPreference @JvmOverloads constructor(
    context : Context,
    attrs : AttributeSet? = null,
    defStyleAttr : Int = TypedArrayUtils.getAttr(
        context, R.attr.dialogPreferenceStyle,
        R.attr.dialogPreferenceStyle
    ),
    defStyleRes : Int = 0
) : DialogPreference(context, attrs, defStyleAttr, defStyleRes) {
    /**
     * The list of entries to be shown in the list in subsequent dialogs
     */
    var entries : Array<CharSequence>?

    /**
     * The array to find the value to save for a preference when an entry from entries is
     * selected. If a user clicks on the second item in entries, the second item in this array
     * will be saved to the preference
     */
    var entryValues : IntArray?

    private var value : Int? = null
        set(value) {
            // Always persist/notify the first time
            val changed = field != value
            if (changed || !isValueSet) {
                field = value
                isValueSet = true
                value?.let { persistInt(it) }
                if (changed)
                    notifyChanged()
            }
        }

    private var isValueSet = false
    val refreshRequired : Boolean

    init {
        val res : Resources = context.resources
        val styledAttrs = context.obtainStyledAttributes(attrs, sR.styleable.IntegerListPreference, defStyleAttr, defStyleRes)

        entries = TypedArrayUtils.getTextArray(styledAttrs, sR.styleable.IntegerListPreference_entries, sR.styleable.IntegerListPreference_android_entries)

        val entryValuesId = TypedArrayUtils.getResourceId(styledAttrs, sR.styleable.IntegerListPreference_android_entryValues, sR.styleable.IntegerListPreference_android_entryValues, 0)
        entryValues = if (entryValuesId != 0) res.getIntArray(entryValuesId) else null

        if (TypedArrayUtils.getBoolean(styledAttrs, sR.styleable.IntegerListPreference_useSimpleSummaryProvider, sR.styleable.IntegerListPreference_useSimpleSummaryProvider, false))
            summaryProvider = SimpleSummaryProvider.instance

        refreshRequired = TypedArrayUtils.getBoolean(styledAttrs, sR.styleable.IntegerListPreference_refreshRequired, sR.styleable.IntegerListPreference_refreshRequired, false)

        styledAttrs.recycle()
    }

    /**
     * @param entriesResId The entries array as a resource
     */
    fun setEntries(@ArrayRes entriesResId : Int) {
        entries = context.resources.getTextArray(entriesResId)
    }

    /**
     * @param entryValuesResId The entry values array as a resource
     */
    fun setEntryValues(@ArrayRes entryValuesResId : Int) {
        entryValues = context.resources.getIntArray(entryValuesResId)
    }

    /**
     * @return The entry corresponding to the current value, or `null`
     */
    fun getEntry() : CharSequence? {
        return entries?.getOrNull(getValueIndex())
    }

    /**
     * @return The index of the value, or -1 if not found
     */
    private fun findIndexOfValue(value : Int?) : Int {
        entryValues?.let {
            if (value != null) {
                for (i in it.indices.reversed()) {
                    if (it[i] == value)
                        return i
                }
            }
        }
        return value ?: -1
    }

    /**
     * Sets the value to the given index from the entry values
     */
    fun setValueIndex(index : Int) {
        value = entryValues?.get(index) ?: index
    }

    private fun getValueIndex() : Int {
        return findIndexOfValue(value)
    }

    override fun onGetDefaultValue(a : TypedArray, index : Int) : Any? {
        return a.getInt(index, 0)
    }

    override fun onSetInitialValue(defaultValue : Any?) {
        // `Preference` superclass passes a null defaultValue if it is sure there is already a persisted value
        // We have to account for that here by passing a random number as default value
        value = if (defaultValue != null)
            getPersistedInt(defaultValue as Int)
        else
            getPersistedInt(0)
    }

    override fun onSaveInstanceState() : Parcelable? {
        val superState = super.onSaveInstanceState()
        if (isPersistent)
            // No need to save instance state since it's persistent
            return superState
        val myState = SavedState(superState)
        myState.value = value
        return myState
    }

    override fun onRestoreInstanceState(state : Parcelable?) {
        if (state == null || state.javaClass != SavedState::class.java) {
            // Didn't save state for us in onSaveInstanceState
            super.onRestoreInstanceState(state)
            return
        }
        val myState = state as SavedState
        super.onRestoreInstanceState(myState.superState)
        value = myState.value
    }

    private class SavedState : BaseSavedState {
        var value : Int? = null

        constructor(source : Parcel) : super(source) {
            value = source.readSerializable() as Int?
        }

        constructor(superState : Parcelable?) : super(superState)

        override fun writeToParcel(dest : Parcel, flags : Int) {
            super.writeToParcel(dest, flags)
            dest.writeSerializable(value)
        }
    }

    /**
     * A simple [androidx.preference.Preference.SummaryProvider] implementation
     * If no value has been set, the summary displayed will be 'Not set', otherwise the summary displayed will be the entry set for this preference
     */
    class SimpleSummaryProvider private constructor() : SummaryProvider<IntegerListPreference> {
        override fun provideSummary(preference : IntegerListPreference) : CharSequence {
            return preference.getEntry() ?: preference.context.getString(R.string.not_set)
        }

        companion object {
            private var simpleSummaryProvider : SimpleSummaryProvider? = null

            val instance : SimpleSummaryProvider?
                get() {
                    if (simpleSummaryProvider == null)
                        simpleSummaryProvider = SimpleSummaryProvider()
                    return simpleSummaryProvider
                }
        }
    }

    open class IntegerListPreferenceDialogFragmentCompat : PreferenceDialogFragmentCompat() {
        var clickedDialogEntryIndex = 0
        private var entries : Array<CharSequence>? = null
        private var entryValues : IntArray? = null

        override fun onCreate(savedInstanceState : Bundle?) {
            super.onCreate(savedInstanceState)
            if (savedInstanceState == null) {
                val preference = listPreference
                check(preference.entries != null) { "IntegerListPreference requires at least the entries array." }
                clickedDialogEntryIndex = preference.findIndexOfValue(preference.value)
                entries = preference.entries
                entryValues = preference.entryValues
            } else {
                clickedDialogEntryIndex = savedInstanceState.getInt(SAVE_STATE_INDEX, 0)
                entries = savedInstanceState.getCharSequenceArray(SAVE_STATE_ENTRIES)
                entryValues = savedInstanceState.getIntArray(SAVE_STATE_ENTRY_VALUES)
            }
        }

        override fun onSaveInstanceState(outState : Bundle) {
            super.onSaveInstanceState(outState)
            outState.putInt(SAVE_STATE_INDEX, clickedDialogEntryIndex)
            outState.putCharSequenceArray(SAVE_STATE_ENTRIES, entries)
            outState.putIntArray(SAVE_STATE_ENTRY_VALUES, entryValues)
        }

        private val listPreference : IntegerListPreference
            get() = preference as IntegerListPreference

        override fun onPrepareDialogBuilder(builder : AlertDialog.Builder) {
            super.onPrepareDialogBuilder(builder)
            builder.setSingleChoiceItems(
                entries, clickedDialogEntryIndex
            ) { dialog, which ->
                if (clickedDialogEntryIndex != which) {
                    clickedDialogEntryIndex = which
                    if (listPreference.refreshRequired)
                        context?.getSettings()?.refreshRequired = true
                }

                // Clicking on an item simulates the positive button click, and dismisses the dialog
                onClick(dialog, DialogInterface.BUTTON_POSITIVE)
                dialog.dismiss()
            }

            // The typical interaction for list-based dialogs is to have click-on-an-item dismiss the dialog instead of the user having to press 'Ok'
            builder.setPositiveButton(null, null)
        }

        override fun onDialogClosed(positiveResult : Boolean) {
            if (positiveResult && clickedDialogEntryIndex >= 0) {
                val value = entryValues?.get(clickedDialogEntryIndex) ?: clickedDialogEntryIndex
                val preference = listPreference
                if (preference.callChangeListener(value))
                    preference.value = value
            }
        }

        companion object {
            private const val SAVE_STATE_INDEX = "IntegerListPreferenceDialogFragment.index"
            private const val SAVE_STATE_ENTRIES = "IntegerListPreferenceDialogFragment.entries"
            private const val SAVE_STATE_ENTRY_VALUES = "IntegerListPreferenceDialogFragment.entryValues"

            fun newInstance(key : String?) : IntegerListPreferenceDialogFragmentCompat {
                val fragment = IntegerListPreferenceDialogFragmentCompat()
                val bundle = Bundle(1)
                bundle.putString(ARG_KEY, key)
                fragment.arguments = bundle
                return fragment
            }
        }
    }
}
