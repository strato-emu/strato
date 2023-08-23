/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package org.stratoemu.strato.fragments

import android.app.Dialog
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.ViewModelProvider
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.snackbar.Snackbar
import org.stratoemu.strato.R
import org.stratoemu.strato.model.TaskViewModel

class IndeterminateProgressDialogFragment : DialogFragment() {
    private val taskViewModel : TaskViewModel by activityViewModels()
    override fun onCreateDialog(savedInstanceState : Bundle?) : Dialog {
        val progressBar = layoutInflater.inflate(R.layout.progress_dialog, null)
        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(requireArguments().getInt(TITLE))
            .setView(progressBar)
            .create()
        dialog.setCanceledOnTouchOutside(false)

        taskViewModel.isComplete.observe(this) { isComplete ->
            if (!isComplete)
                return@observe

            dialog.dismiss()
            taskViewModel.clear()
        }

        if (taskViewModel.isRunning.value == false) {
            taskViewModel.runTask()
        }

        return dialog
    }

    companion object {
        const val TAG = "IndeterminateProgressDialogFragment"

        private const val TITLE = "Title"
        fun newInstance(activity : AppCompatActivity, titleId : Int, task : () -> Unit) : IndeterminateProgressDialogFragment {
            val dialog = IndeterminateProgressDialogFragment()
            val bundle = Bundle(1)
            bundle.putInt(TITLE, titleId)
            ViewModelProvider(activity)[TaskViewModel::class.java].task = task
            dialog.arguments = bundle
            return dialog
        }
    }
}
