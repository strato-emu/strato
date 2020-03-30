/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.os.Bundle
import android.text.method.ScrollingMovementMethod
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import emu.skyline.R
import kotlinx.android.synthetic.main.license_dialog.*

class LicenseDialog : DialogFragment() {
    private var libraryUrl: String = ""
    private var libraryLicense: String = ""

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        libraryUrl = arguments?.getString("libraryUrl")!!
        libraryLicense = context?.getString(arguments?.getInt("libraryLicense")!!)!!

        return requireActivity().layoutInflater.inflate(R.layout.license_dialog, container)
    }

    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        license_url.text = libraryUrl
        license_content.text = libraryLicense
        license_content.movementMethod = ScrollingMovementMethod()
    }
}
