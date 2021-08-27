/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import java.lang.Exception
import android.os.Bundle
import android.os.Parcel
import android.os.Parcelable
import android.os.Parcelable.Creator
import android.util.AttributeSet
import android.content.Context
import android.content.res.Resources
import android.content.res.TypedArray
import android.content.DialogInterface
import android.annotation.SuppressLint
import androidx.annotation.ArrayRes
import androidx.appcompat.app.AlertDialog
import androidx.core.content.res.TypedArrayUtils
import androidx.preference.R
import androidx.preference.DialogPreference
import androidx.preference.PreferenceDialogFragmentCompat

/**
 * A Preference that displays a list of entries as a dialog.
 * This preference saves an integer value instead of a string one.
 * @see androidx.preference.ListPreference
 */
@SuppressLint("RestrictedApi", "ResourceType")
class IntegerListPreference @JvmOverloads constructor(
    context : Context,
    attrs : AttributeSet? = null,
    defStyleAttr : Int = TypedArrayUtils.getAttr(
        context, R.attr.dialogPreferenceStyle,
        R.attr.dialogPreferenceStyle
    ),
    defStyleRes : Int = 0
) : DialogPreference(context, attrs, defStyleAttr, defStyleRes) {
    /**
     * The list of entries to be shown in the list in subsequent dialogs.
     */
    var entries : Array<CharSequence>?

    /**
     * The array to find the value to save for a preference when an entry from entries is
     * selected. If a user clicks on the second item in entries, the second item in this array
     * will be saved to the preference.
     */
    var entryValues : IntArray?

    private var value : Int? = null
        set(value) {
            // Always persist/notify the first time.
            val changed = field != value
            if (changed || !isValueSet) {
                field = value
                isValueSet = true
                value?.let { persistInt(it) }
                if (changed) {
                    notifyChanged()
                }
            }
        }

    private var isValueSet = false

    init {
        val res : Resources = context.resources
        val entryValuesId = attrs!!.getAttributeResourceValue("http://schemas.android.com/apk/res/android", "entryValues", 0)

        var a = context.obtainStyledAttributes(
            attrs, R.styleable.ListPreference, defStyleAttr, defStyleRes
        )
        entries = TypedArrayUtils.getTextArray(
            a, R.styleable.ListPreference_entries,
            R.styleable.ListPreference_android_entries
        )
        entryValues = try { res.getIntArray(entryValuesId) } catch (e : Exception) { null }
        if (TypedArrayUtils.getBoolean(
                a, R.styleable.ListPreference_useSimpleSummaryProvider,
                R.styleable.ListPreference_useSimpleSummaryProvider, false
            )
        ) {
            summaryProvider = SimpleSummaryProvider.instance
        }
        a.recycle()

        //Retrieve the Preference summary attribute since it's private in the Preference class.
        a = context.obtainStyledAttributes(
            attrs,
            R.styleable.Preference, defStyleAttr, defStyleRes
        )
        a.recycle()
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
     * @param value The value whose index should be returned
     * @return The index of the value, or -1 if not found
     */
    private fun findIndexOfValue(value : Int?) : Int {
        entryValues?.let {
            if (value != null) {
                for (i in it.indices.reversed()) {
                    if (it[i] == value) {
                        return i
                    }
                }
            }
        }
        return value ?: -1
    }

    /**
     * Sets the value to the given index from the entry values.
     * @param index The index of the value to set
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
        // `Preference` superclass passes a null defaultValue if it is sure there
        // is already a persisted value, se we have to account for that here by
        // passing a random number as default value.
        value = if (defaultValue != null) {
            getPersistedInt(defaultValue as Int)
        } else {
            getPersistedInt(0)
        }
    }

    override fun onSaveInstanceState() : Parcelable {
        val superState = super.onSaveInstanceState()
        if (isPersistent) {
            // No need to save instance state since it's persistent
            return superState
        }
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

        companion object {
            @JvmField
            val CREATOR : Creator<SavedState> = object : Creator<SavedState> {
                override fun createFromParcel(input : Parcel) : SavedState? {
                    return SavedState(input)
                }

                override fun newArray(size : Int) : Array<SavedState?> {
                    return arrayOfNulls(size)
                }
            }
        }
    }

    /**
     * A simple [androidx.preference.Preference.SummaryProvider] implementation for a
     * [IntegerListPreference]. If no value has been set, the summary displayed will be 'Not set',
     * otherwise the summary displayed will be the entry set for this preference.
     */
    class SimpleSummaryProvider private constructor() : SummaryProvider<IntegerListPreference> {
        override fun provideSummary(preference : IntegerListPreference) : CharSequence {
            return preference.getEntry() ?: preference.context.getString(R.string.not_set)
        }

        companion object {
            private var simpleSummaryProvider : SimpleSummaryProvider? = null

            /**
             * Retrieve a singleton instance of this simple
             * [androidx.preference.Preference.SummaryProvider] implementation.
             *
             * @return a singleton instance of this simple
             * [androidx.preference.Preference.SummaryProvider] implementation
             */
            val instance : SimpleSummaryProvider?
                get() {
                    if (simpleSummaryProvider == null) {
                        simpleSummaryProvider = SimpleSummaryProvider()
                    }
                    return simpleSummaryProvider
                }
        }
    }

    class IntegerListPreferenceDialogFragmentCompat : PreferenceDialogFragmentCompat() {
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
                clickedDialogEntryIndex = which

                // Clicking on an item simulates the positive button click, and dismisses
                // the dialog.
                onClick(
                    dialog,
                    DialogInterface.BUTTON_POSITIVE
                )
                dialog.dismiss()
            }

            // The typical interaction for list-based dialogs is to have click-on-an-item dismiss the
            // dialog instead of the user having to press 'Ok'.
            builder.setPositiveButton(null, null)
        }

        override fun onDialogClosed(positiveResult : Boolean) {
            if (positiveResult && clickedDialogEntryIndex >= 0) {
                val value = entryValues?.get(clickedDialogEntryIndex) ?: clickedDialogEntryIndex
                val preference = listPreference
                if (preference.callChangeListener(value)) {
                    preference.value = value
                }
            }
        }

        companion object {
            private const val SAVE_STATE_INDEX = "ListPreferenceDialogFragment.index"
            private const val SAVE_STATE_ENTRIES = "ListPreferenceDialogFragment.entries"
            private const val SAVE_STATE_ENTRY_VALUES = "ListPreferenceDialogFragment.entryValues"

            fun newInstance(key : String?) : IntegerListPreferenceDialogFragmentCompat {
                val fragment = IntegerListPreferenceDialogFragmentCompat()
                val b = Bundle(1)
                b.putString(ARG_KEY, key)
                fragment.arguments = b
                return fragment
            }
        }
    }
}