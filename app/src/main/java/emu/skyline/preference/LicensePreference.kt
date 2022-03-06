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
class LicensePreference @JvmOverloads constructor(context : Context, attrs : AttributeSet? = null, defStyleAttr : Int = R.attr.dialogPreferenceStyle) : Preference(context, attrs, defStyleAttr) {
    /**
     * The [FragmentManager] is used to show the [LicenseDialog] fragment
     */
    private val fragmentManager = (context as AppCompatActivity).supportFragmentManager

    companion object {
        const val LIBRARY_TITLE_ARG = "libraryTitle"
        const val LIBRARY_URL_ARG = "libraryUrl"
        const val LIBRARY_COPYRIGHT_ARG = "libraryCopyright"
        const val LIBRARY_LICENSE_ARG = "libraryLicense"

        private val DIALOG_TAG = LicensePreference::class.java.simpleName
    }

    /**
     * The copyright notice of the library
     */
    private var libraryCopyright : String? = null

    /**
     * The URL of the library
     */
    private lateinit var libraryUrl : String

    /**
     * The contents of the license of this library
     */
    private var libraryLicense = 0

    init {
        for (i in 0 until attrs!!.attributeCount) {
            when (attrs.getAttributeName(i)) {
                LIBRARY_URL_ARG -> libraryUrl = attrs.getAttributeValue(i)
                LIBRARY_COPYRIGHT_ARG -> libraryCopyright = attrs.getAttributeValue(i)
                LIBRARY_LICENSE_ARG -> libraryLicense = attrs.getAttributeValue(i).substring(1).toInt()
            }
        }
    }

    /**
     * The [LicenseDialog] fragment is shown using [fragmentManager] on click with [libraryUrl] and [libraryLicense] passed as arguments
     */
    override fun onClick() {
        fragmentManager.findFragmentByTag(DIALOG_TAG) ?: run {
            LicenseDialog().apply {
                arguments = Bundle().apply {
                    putString(LIBRARY_TITLE_ARG, title.toString())
                    putString(LIBRARY_URL_ARG, libraryUrl)
                    putString(LIBRARY_COPYRIGHT_ARG, libraryCopyright)
                    putInt(LIBRARY_LICENSE_ARG, libraryLicense)
                }
            }.show(fragmentManager, DIALOG_TAG)
        }
    }
}
