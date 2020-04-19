/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.utility

import android.content.ComponentName
import android.content.Intent
import android.content.pm.ShortcutInfo
import android.content.pm.ShortcutManager
import android.graphics.drawable.Icon
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.graphics.drawable.toBitmap
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import emu.skyline.EmulationActivity
import emu.skyline.R
import emu.skyline.adapter.AppItem
import kotlinx.android.synthetic.main.app_dialog.*

/**
 * This dialog is used to show extra game metadata and provide extra options such as pinning the game to the home screen
 *
 * @param item This is used to hold the [AppItem] between instances
 */
class AppDialog(val item: AppItem? = null) : BottomSheetDialogFragment() {

    /**
     * This inflates the layout of the dialog after initial view creation
     */
    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return requireActivity().layoutInflater.inflate(R.layout.app_dialog, container)
    }

    /**
     * This expands the bottom sheet so that it's fully visible
     */
    override fun onStart() {
        super.onStart()

        val behavior = BottomSheetBehavior.from(requireView().parent as View)
        behavior.state = BottomSheetBehavior.STATE_EXPANDED
    }

    /**
     * This fills all the dialog with the information from [item] if it is valid and setup all user interaction
     */
    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)

        if (item is AppItem) {
            val missingIcon = context?.resources?.getDrawable(R.drawable.default_icon, context?.theme)?.toBitmap(256, 256)

            game_icon.setImageBitmap(item.icon ?: missingIcon)
            game_title.text = item.title
            game_subtitle.text = item.subTitle ?: getString(R.string.metadata_missing)

            val shortcutManager = activity?.getSystemService(ShortcutManager::class.java)!!
            game_pin.isEnabled = shortcutManager.isRequestPinShortcutSupported

            game_pin.setOnClickListener {
                val info = ShortcutInfo.Builder(context, item.title)
                info.setShortLabel(item.meta.name)
                info.setActivity(ComponentName(context!!, EmulationActivity::class.java))
                info.setIcon(Icon.createWithBitmap(item.icon ?: missingIcon))

                val intent = Intent(context, EmulationActivity::class.java)
                intent.data = item.uri
                intent.action = Intent.ACTION_VIEW

                info.setIntent(intent)

                shortcutManager.requestPinShortcut(info.build(), null)
            }

            game_play.setOnClickListener {
                val intent = Intent(activity, EmulationActivity::class.java)
                intent.data = item.uri

                startActivity(intent)
            }
        } else
            activity?.supportFragmentManager?.beginTransaction()?.remove(this)?.commit()
    }
}
