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
import android.view.KeyEvent
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.data.AppItem
import emu.skyline.loader.LoaderResult
import kotlinx.android.synthetic.main.app_dialog.*

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
            args.putSerializable("item", item)

            val fragment = AppDialog()
            fragment.arguments = args
            return fragment
        }
    }

    private lateinit var item : AppItem

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater : LayoutInflater, container : ViewGroup?, savedInstanceState : Bundle?) : View? {
        return requireActivity().layoutInflater.inflate(R.layout.app_dialog, container)
    }

    override fun onCreate(savedInstanceState : Bundle?) {
        super.onCreate(savedInstanceState)

        item = requireArguments().getSerializable("item") as AppItem
    }

    /**
     * This expands the bottom sheet so that it's fully visible and map the B button to back
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED

        dialog?.setOnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_BUTTON_B && event.action == KeyEvent.ACTION_UP) {
                dialog?.onBackPressed()
                return@setOnKeyListener true
            }
            false
        }
    }

    /**
     * This fills all the dialog with the information from [item] if it is valid and setup all user interaction
     */
    override fun onActivityCreated(savedInstanceState : Bundle?) {
        super.onActivityCreated(savedInstanceState)

        val missingIcon = ContextCompat.getDrawable(requireActivity(), R.drawable.default_icon)!!.toBitmap(256, 256)

        game_icon.setImageBitmap(item.icon ?: missingIcon)
        game_title.text = item.title
        game_subtitle.text = item.subTitle ?: item.loaderResultString(requireContext())

        game_play.isEnabled = item.loaderResult == LoaderResult.Success
        game_play.setOnClickListener {
            startActivity(Intent(activity, EmulationActivity::class.java).apply { data = item.uri })
        }

        val shortcutManager = requireActivity().getSystemService(ShortcutManager::class.java)
        game_pin.isEnabled = shortcutManager.isRequestPinShortcutSupported

        game_pin.setOnClickListener {
            val info = ShortcutInfo.Builder(context, item.title)
            info.setShortLabel(item.meta.name)
            info.setActivity(ComponentName(requireContext(), EmulationActivity::class.java))
            info.setIcon(Icon.createWithAdaptiveBitmap(item.icon ?: missingIcon))

            val intent = Intent(context, EmulationActivity::class.java)
            intent.data = item.uri
            intent.action = Intent.ACTION_VIEW

            info.setIntent(intent)

            shortcutManager.requestPinShortcut(info.build(), null)
        }
    }
}
