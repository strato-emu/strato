/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato

import android.content.ComponentName
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.drawable.Icon
import android.os.Bundle
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.ActivityResultLauncher
import androidx.appcompat.app.AlertDialog
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.snackbar.Snackbar
import org.stratoemu.strato.data.BaseAppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.databinding.AppDialogBinding
import org.stratoemu.strato.loader.LoaderResult
import org.stratoemu.strato.settings.SettingsActivity
import org.stratoemu.strato.utils.SaveManagementUtils
import org.stratoemu.strato.utils.serializable

/**
 * This dialog is used to show extra game metadata and provide extra options such as pinning the game to the home screen
 */
class AppDialog : BottomSheetDialogFragment() {
    companion object {
        /**
         * @param item This is used to hold the [BaseAppItem] between instances
         */
        fun newInstance(item : BaseAppItem) : AppDialog {
            val args = Bundle()
            args.putSerializable(AppItemTag, item)

            val fragment = AppDialog()
            fragment.arguments = args
            return fragment
        }
    }

    private lateinit var binding : AppDialogBinding

    private val item by lazy { requireArguments().serializable<BaseAppItem>(AppItemTag)!! }

    /**
     * Used to manage save files
     */
    private lateinit var documentPicker : ActivityResultLauncher<Array<String>>
    private lateinit var startForResultExportSave : ActivityResultLauncher<Intent>

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)
        documentPicker = SaveManagementUtils.registerDocumentPicker(requireActivity()) {
            val isSaveFileOfThisGame = SaveManagementUtils.saveFolderGameExists(item.titleId)
            binding.deleteSave.isEnabled = isSaveFileOfThisGame
            binding.exportSave.isEnabled = isSaveFileOfThisGame
        }
        startForResultExportSave = SaveManagementUtils.registerStartForResultExportSave(requireActivity())
    }

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) = AppDialogBinding.inflate(inflater).also { binding = it }.root

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Set the peek height after the root view has been laid out
        view.apply {
            post {
                val behavior = BottomSheetBehavior.from(parent as View)
                behavior.peekHeight = height
            }
        }

        binding.gameIcon.setImageBitmap(item.bitmapIcon)
        binding.gameTitle.text = item.title
        binding.gameVersion.text = item.version ?: item.loaderResultString(requireContext())
        binding.gameTitleId.text = item.titleId
        binding.gameAuthor.text = item.author

        binding.gamePlay.isEnabled = item.loaderResult == LoaderResult.Success
        binding.gamePlay.setOnClickListener {
            startActivity(Intent(activity, EmulationActivity::class.java).apply {
                putExtras(requireArguments())
            })
        }

        binding.gameSettings.isEnabled = item.loaderResult == LoaderResult.Success
        binding.gameSettings.setOnClickListener {
            startActivity(Intent(activity, SettingsActivity::class.java).apply {
                putExtras(requireArguments())
            })
        }

        val shortcutManager = requireActivity().getSystemService(ShortcutManager::class.java)
        binding.gamePin.isEnabled = shortcutManager.isRequestPinShortcutSupported

        binding.gamePin.setOnClickListener {
            val info = ShortcutInfo.Builder(context, item.title)
            item.title?.let { title -> info.setShortLabel(title) }
            info.setActivity(ComponentName(requireContext(), EmulationActivity::class.java))
            info.setIcon(Icon.createWithAdaptiveBitmap(item.bitmapIcon))

            val intent = Intent(context, EmulationActivity::class.java)
            intent.data = item.uri
            intent.action = Intent.ACTION_VIEW

            info.setIntent(intent)

            shortcutManager.requestPinShortcut(info.build(), null)
        }

        val saveExists = SaveManagementUtils.saveFolderGameExists(item.titleId)

        binding.deleteSave.isEnabled = saveExists
        binding.deleteSave.setOnClickListener {
            AlertDialog.Builder(requireContext())
                .setTitle(getString(R.string.delete_save_confirmation_message))
                .setMessage(getString(R.string.action_irreversible))
                .setNegativeButton(getString(R.string.no), null)
                .setPositiveButton(getString(R.string.yes)) { _, _ ->
                    SaveManagementUtils.deleteSaveFile(item.titleId)
                    binding.deleteSave.isEnabled = false
                    binding.exportSave.isEnabled = false
                }.show()
        }

        binding.importSave.setOnClickListener {
            SaveManagementUtils.importSave(documentPicker)
        }

        binding.exportSave.isEnabled = saveExists
        binding.exportSave.setOnClickListener {
            SaveManagementUtils.exportSave(requireContext(), startForResultExportSave, item.titleId, "${item.title} (v${binding.gameVersion.text}) [${item.titleId}]")
        }

        binding.gameTitleId.setOnLongClickListener {
            val clipboard = requireActivity().getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
            clipboard.setPrimaryClip(android.content.ClipData.newPlainText("Title ID", item.titleId))
            Snackbar.make(binding.root, getString(R.string.copied_to_clipboard), Snackbar.LENGTH_SHORT).show()
            true
        }

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
