/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.preference

import android.graphics.Rect
import android.os.Bundle
import android.view.*
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.databinding.LicenseDialogBinding

/**
 * Dialog for displaying the contents of a license for a particular library
 */
class LicenseDialog : BottomSheetDialogFragment() {
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

        // Set the peek height after the root view has been laid out
        view.apply {
            post {
                val behavior = BottomSheetBehavior.from(parent as View)
                behavior.peekHeight = (height * 0.7).toInt()
            }
        }

        binding.libraryTitle.text = requireArguments().getString(LicensePreference.LIBRARY_TITLE_ARG)
        binding.libraryUrl.text = requireArguments().getString(LicensePreference.LIBRARY_URL_ARG)
        binding.libraryCopyright.text = requireArguments().getString(LicensePreference.LIBRARY_COPYRIGHT_ARG)
        if (binding.libraryCopyright.text.isEmpty())
            binding.libraryCopyright.visibility = View.GONE
        binding.licenseContent.text = getString(requireArguments().getInt(LicensePreference.LIBRARY_LICENSE_ARG))

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
