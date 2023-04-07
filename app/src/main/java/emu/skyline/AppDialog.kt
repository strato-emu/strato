/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline

import android.content.ComponentName
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.drawable.Icon
import android.os.Bundle
import android.provider.DocumentsContract
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.documentfile.provider.DocumentFile
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.snackbar.Snackbar
import emu.skyline.data.AppItem
import emu.skyline.data.AppItemTag
import emu.skyline.databinding.AppDialogBinding
import emu.skyline.loader.LoaderResult
import emu.skyline.provider.DocumentsProvider
import emu.skyline.settings.SettingsActivity
import emu.skyline.utils.ZipUtils
import emu.skyline.utils.serializable
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedOutputStream
import java.io.File
import java.io.FileOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * This dialog is used to show extra game metadata and provide extra options such as pinning the game to the home screen
 */
class AppDialog : BottomSheetDialogFragment() {
    companion object {
        /**
         * @param item This is used to hold the [AppItem] between instances
         */
        fun newInstance(item : AppItem) : AppDialog {
            val args = Bundle()
            args.putSerializable(AppItemTag, item)

            val fragment = AppDialog()
            fragment.arguments = args
            return fragment
        }
    }

    private lateinit var binding : AppDialogBinding

    private val item by lazy { requireArguments().serializable<AppItem>(AppItemTag)!! }

    private val savesFolderRoot by lazy { "${requireContext().getPublicFilesDir().canonicalPath}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001/" }
    private val documentPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) {
        it?.let { uri ->
            if (uri.toString().takeLast(20).removeSuffix(".zip") == item.titleId) {
                val saveFolder = File(savesFolderRoot + item.titleId)
                val inputZip = requireContext().contentResolver.openInputStream(uri)
                if (inputZip != null) {
                    CoroutineScope(Dispatchers.IO).launch {
                        ZipUtils.unzip(inputZip, saveFolder)
                        withContext(Dispatchers.Main){
                            binding.deleteSave.isEnabled = true
                            binding.exportSave.isEnabled = true
                        }
                    }
                }
            } else {
                Snackbar.make(binding.root, "Zip file must have as name the TitleID of the game", Snackbar.LENGTH_LONG).show()
            }
        }
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
            info.setShortLabel(item.title)
            info.setActivity(ComponentName(requireContext(), EmulationActivity::class.java))
            info.setIcon(Icon.createWithAdaptiveBitmap(item.bitmapIcon))

            val intent = Intent(context, EmulationActivity::class.java)
            intent.data = item.uri
            intent.action = Intent.ACTION_VIEW

            info.setIntent(intent)

            shortcutManager.requestPinShortcut(info.build(), null)
        }

        val saveFolderPath = savesFolderRoot + item.titleId
        val saveExists = File(saveFolderPath).exists()

        binding.deleteSave.isEnabled = saveExists
        binding.deleteSave.setOnClickListener {
            AlertDialog.Builder(requireContext())
                .setTitle("Are you sure you want to delete this save?")
                .setMessage("This action is irreversible.")
                .setPositiveButton("Yes") { dialogInterface, _ ->
                    File(saveFolderPath).deleteRecursively()
                    binding.deleteSave.isEnabled = false
                    binding.exportSave.isEnabled = false
                    dialogInterface.dismiss()
                }.setNegativeButton("No") { dialogInterface, _ ->
                    dialogInterface.cancel()
                }.show()
        }

        binding.importSave.setOnClickListener {
            documentPicker.launch(arrayOf("application/zip"))
        }

        binding.exportSave.isEnabled = saveExists
        binding.exportSave.setOnClickListener {
            CoroutineScope(Dispatchers.IO).launch {
                val saveFolder = File(saveFolderPath)
                //val outputZipFile = File.createTempFile("out", ".zip")
                val outputZipFile = File("$saveFolderPath.zip")
                if (outputZipFile.exists()) outputZipFile.delete()
                outputZipFile.createNewFile()
                ZipOutputStream(BufferedOutputStream(FileOutputStream(outputZipFile))).use { zos ->
                    saveFolder.walkTopDown().forEach { file ->
                        val zipFileName = file.absolutePath.removePrefix(saveFolder.absolutePath).removePrefix("/")
                        if (zipFileName == "") return@forEach
                        val entry = ZipEntry("$zipFileName${(if (file.isDirectory) "/" else "")}")
                        zos.putNextEntry(entry)
                        if (file.isFile) {
                            file.inputStream().use { fis -> fis.copyTo(zos) }
                        }
                    }
                }
                withContext(Dispatchers.Main) {
                    val file = DocumentFile.fromSingleUri(requireContext(), DocumentsContract.buildDocumentUri(DocumentsProvider.AUTHORITY, "${DocumentsProvider.ROOT_ID}/switch/nand/user/save/0000000000000000/00000000000000000000000000000001/${item.titleId}.zip"))!!
                    val intent = Intent(Intent.ACTION_SEND)
                        .setDataAndType(file.uri, "application/zip")
                        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
                        .putExtra(Intent.EXTRA_STREAM, file.uri)
                    startActivity(Intent.createChooser(intent, "Share save file"))
                }
            }
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

    override fun onDestroyView(){
        super.onDestroyView()
        File("$savesFolderRoot${item.titleId}.zip").delete()
    }
}
