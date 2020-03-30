/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment.STYLE_NORMAL
import androidx.fragment.app.FragmentManager
import androidx.preference.Preference
import emu.skyline.R
import emu.skyline.SettingsActivity

class LicensePreference : Preference {
    private val fragmentManager: FragmentManager
    private val mDialogFragmentTag = "LicensePreference"
    private var libraryUrl: String? = null
    private var libraryLicense: Int? = null

    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int, defStyleRes: Int) : super(context, attrs, defStyleAttr, defStyleRes) {
        fragmentManager = (context as AppCompatActivity).supportFragmentManager

        for (i in 0 until attrs!!.attributeCount) {
            val attr = attrs.getAttributeName(i)
            if (attr.equals("libraryUrl", ignoreCase = true))
                libraryUrl = attrs.getAttributeValue(i)
            else if (attr.equals("libraryLicense", ignoreCase = true))
                libraryLicense = attrs.getAttributeValue(i).substring(1).toInt()
        }
    }

    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : this(context, attrs, defStyleAttr, R.style.LicenseDialogTheme)

    constructor(context: Context?, attrs: AttributeSet?) : this(context, attrs, R.attr.dialogPreferenceStyle)

    constructor(context: Context?) : this(context, null)

    override fun onClick() {
        if (fragmentManager.findFragmentByTag(mDialogFragmentTag) != null)
            return

        val fragment = LicenseDialog()

        val bundle = Bundle(2)
        bundle.putString("libraryUrl", libraryUrl!!)
        bundle.putInt("libraryLicense", libraryLicense!!)
        fragment.arguments = bundle

        fragment.setStyle(STYLE_NORMAL, R.style.LicenseDialogTheme)

        fragment.show(fragmentManager, mDialogFragmentTag)
    }
}
