/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

@file:SuppressLint("RestrictedApi")

package skyline.strato.preference.dialog

import android.annotation.SuppressLint
import android.app.Dialog
import android.os.Build
import android.os.Bundle
import android.view.WindowInsets
import androidx.preference.EditTextPreferenceDialogFragmentCompat
import com.google.android.material.dialog.MaterialAlertDialogBuilder

/**
 * An [EditTextPreferenceDialogFragmentCompat] that uses [MaterialAlertDialogBuilder]
 */
class EditTextPreferenceMaterialDialogFragmentCompat : EditTextPreferenceDialogFragmentCompat() {
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
        fun newInstance(key : String?) : EditTextPreferenceMaterialDialogFragmentCompat {
            val fragment = EditTextPreferenceMaterialDialogFragmentCompat()
            val bundle = Bundle(1)
            bundle.putString(ARG_KEY, key)
            fragment.arguments = bundle
            return fragment
        }
    }
}
