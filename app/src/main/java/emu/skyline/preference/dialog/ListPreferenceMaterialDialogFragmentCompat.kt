/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

@file:SuppressLint("RestrictedApi")

package emu.skyline.preference.dialog

import android.annotation.SuppressLint
import android.app.Dialog
import android.os.Build
import android.os.Bundle
import android.view.WindowInsets
import androidx.preference.ListPreferenceDialogFragmentCompat
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/**
 * A [ListPreferenceDialogFragmentCompat] that uses [MaterialAlertDialogBuilder]
 */
class ListPreferenceMaterialDialogFragmentCompat : ListPreferenceDialogFragmentCompat() {
    override fun onCreateDialog(savedInstanceState : Bundle?) : Dialog {
        val builder = MaterialAlertDialogBuilder(requireContext())
            .setTitle(preference.dialogTitle)
            .setIcon(preference.dialogIcon)
            .setPositiveButton(preference.positiveButtonText, this)
            .setNegativeButton(preference.negativeButtonText, this)

        val contentView = onCreateDialogView(requireContext())
        if (contentView != null) {
            onBindDialogView(contentView)
            builder.setView(contentView)
        } else {
            builder.setMessage(preference.dialogMessage)
        }

        onPrepareDialogBuilder(builder)

        // Create the dialog
        val dialog : Dialog = builder.create()
        if (needInputMethod()) {
            requestInputMethod(dialog)
        }

        return dialog
    }

    private fun requestInputMethod(dialog : Dialog) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            dialog.window?.decorView?.windowInsetsController?.show(WindowInsets.Type.ime())
        else
            scheduleShowSoftInput()
    }

    companion object {
        fun newInstance(key : String?) : ListPreferenceMaterialDialogFragmentCompat {
            val fragment = ListPreferenceMaterialDialogFragmentCompat()
            val bundle = Bundle(1)
            bundle.putString(ARG_KEY, key)
            fragment.arguments = bundle
            return fragment
        }
    }
}
