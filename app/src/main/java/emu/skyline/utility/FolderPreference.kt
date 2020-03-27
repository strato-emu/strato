/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.TypedArray
import android.net.Uri
import android.os.Parcel
import android.os.Parcelable
import android.text.TextUtils
import android.util.AttributeSet
import androidx.preference.Preference

class FolderPreference : Preference {
    private var mDirectory: String? = null

    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(context, attrs, defStyleAttr) {
        summaryProvider = SimpleSummaryProvider.instance
    }

    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs) {
        summaryProvider = SimpleSummaryProvider.instance
    }

    constructor(context: Context?) : super(context) {
        summaryProvider = SimpleSummaryProvider.instance
    }

    override fun onClick() {
        val intent = Intent(context, FolderActivity::class.java)
        (context as Activity).startActivityForResult(intent, 0)
    }

    var directory: String?
        get() = mDirectory
        set(directory) {
            val changed = !TextUtils.equals(mDirectory, directory)
            if (changed) {
                mDirectory = directory
                persistString(directory)
                if (changed) {
                    notifyChanged()
                }
            }
        }

    override fun onGetDefaultValue(a: TypedArray, index: Int): Any {
        return a.getString(index)!!
    }

    override fun onSetInitialValue(defaultValue: Any?) {
        directory = getPersistedString(defaultValue as String?)
    }

    override fun onSaveInstanceState(): Parcelable {
        val superState = super.onSaveInstanceState()
        if (isPersistent) {
            return superState
        }
        val myState = SavedState(superState)
        myState.mDirectory = directory
        return myState
    }

    override fun onRestoreInstanceState(state: Parcelable?) {
        if (state == null || state.javaClass != SavedState::class.java) {
            super.onRestoreInstanceState(state)
            return
        }
        val myState = state as SavedState
        super.onRestoreInstanceState(myState.superState)
        directory = myState.mDirectory
    }

    internal class SavedState : BaseSavedState {
        var mDirectory: String? = null

        constructor(source: Parcel) : super(source) {
            mDirectory = source.readString()
        }

        constructor(superState: Parcelable?) : super(superState)

        override fun writeToParcel(dest: Parcel, flags: Int) {
            super.writeToParcel(dest, flags)
            dest.writeString(mDirectory)
        }

        override fun describeContents(): Int {
            return 0
        }
    }

    class SimpleSummaryProvider private constructor() : SummaryProvider<FolderPreference> {
        override fun provideSummary(preference: FolderPreference): CharSequence {
            return Uri.decode(preference.directory!!)
        }

        companion object {
            private var sSimpleSummaryProvider: SimpleSummaryProvider? = null
            val instance: SimpleSummaryProvider?
                get() {
                    if (sSimpleSummaryProvider == null) {
                        sSimpleSummaryProvider = SimpleSummaryProvider()
                    }
                    return sSimpleSummaryProvider
                }
        }
    }
}
