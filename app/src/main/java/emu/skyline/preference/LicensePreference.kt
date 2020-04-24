/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.content.Context
import android.os.Bundle
import android.util.AttributeSet
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.FragmentManager
import androidx.preference.Preference
import emu.skyline.R

/**
 * This preference is used to show licenses and the source of a library
 */
class LicensePreference : Preference {
    /**
     * The [FragmentManager] is used to show the [LicenseDialog] fragment
     */
    private val fragmentManager : FragmentManager

    /**
     * The tag used by this preference when launching a corresponding fragment
     */
    private val mDialogFragmentTag = "LicensePreference"

    /**
     * The URL of the library
     */
    private var libraryUrl : String? = null

    /**
     * The contents of the license of this library
     */
    private var libraryLicense : Int? = null

    /**
     * The constructor assigns the [fragmentManager] from the activity and finds [libraryUrl] and [libraryLicense] in the attributes
     */
    constructor(context : Context?, attrs : AttributeSet?, defStyleAttr : Int, defStyleRes : Int) : super(context, attrs, defStyleAttr, defStyleRes) {
        fragmentManager = (context as AppCompatActivity).supportFragmentManager

        for (i in 0 until attrs!!.attributeCount) {
            val attr = attrs.getAttributeName(i)

            if (attr.equals("libraryUrl", ignoreCase = true))
                libraryUrl = attrs.getAttributeValue(i)
            else if (attr.equals("libraryLicense", ignoreCase = true))
                libraryLicense = attrs.getAttributeValue(i).substring(1).toInt()
        }
    }

    constructor(context : Context?, attrs : AttributeSet?, defStyleAttr : Int) : this(context, attrs, defStyleAttr, 0)

    constructor(context : Context?, attrs : AttributeSet?) : this(context, attrs, R.attr.dialogPreferenceStyle)

    constructor(context : Context?) : this(context, null)

    /**
     * The [LicenseDialog] fragment is shown using [fragmentManager] on click with [libraryUrl] and [libraryLicense] passed as arguments
     */
    override fun onClick() {
        if (fragmentManager.findFragmentByTag(mDialogFragmentTag) != null)
            return

        val dialog = LicenseDialog()

        val bundle = Bundle(2)
        bundle.putString("libraryUrl", libraryUrl!!)
        bundle.putInt("libraryLicense", libraryLicense!!)
        dialog.arguments = bundle

        dialog.show(fragmentManager, mDialogFragmentTag)
    }
}
