/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.graphics.Rect
import android.os.Bundle
import android.view.*
import androidx.fragment.app.DialogFragment
import emu.skyline.R
import kotlinx.android.synthetic.main.license_dialog.*

/**
 * This dialog is used to display the contents of a license for a particular project
 */
class LicenseDialog : DialogFragment() {
    /**
     * This inflates the layout of the dialog and sets the minimum width/height to 90% of the screen size
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? {
        val layout = layoutInflater.inflate(R.layout.license_dialog, container)

        val displayRectangle = Rect()
        val window : Window = requireActivity().window
        window.decorView.getWindowVisibleDisplayFrame(displayRectangle)

        layout.minimumWidth = ((displayRectangle.width() * 0.9f).toInt())
        layout.minimumHeight = ((displayRectangle.height() * 0.9f).toInt())

        return layout
    }

    /**
     * This sets the [license_url] and [license_content] based on arguments passed
     */
    override fun onActivityCreated(savedInstanceState : Bundle?) {
        super.onActivityCreated(savedInstanceState)

        license_url.text = arguments?.getString("libraryUrl")!!
        license_content.text = context?.getString(arguments?.getInt("libraryLicense")!!)!!

        dialog?.setOnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_BUTTON_B && event.action == KeyEvent.ACTION_UP) {
                dialog?.onBackPressed()
                true
            } else {
                false
            }
        }
    }
}
