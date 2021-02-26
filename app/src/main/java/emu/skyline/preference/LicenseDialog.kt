/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.graphics.Rect
import android.os.Bundle
import android.view.*
import androidx.fragment.app.DialogFragment
import emu.skyline.databinding.LicenseDialogBinding

/**
 * This dialog is used to display the contents of a license for a particular project
 */
class LicenseDialog : DialogFragment() {
    private lateinit var binding : LicenseDialogBinding

    /**
     * This inflates the layout of the dialog and sets the minimum width/height to 90% of the screen size
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? {
        val displayRectangle = Rect()
        val window : Window = requireActivity().window
        window.decorView.getWindowVisibleDisplayFrame(displayRectangle)

        return LicenseDialogBinding.inflate(inflater).apply {
            root.minimumWidth = ((displayRectangle.width() * 0.9f).toInt())
            root.minimumHeight = ((displayRectangle.height() * 0.9f).toInt())
            binding = this
        }.root
    }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.licenseUrl.text = requireArguments().getString("libraryUrl")
        binding.licenseContent.text = getString(requireArguments().getInt("libraryLicense"))

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
