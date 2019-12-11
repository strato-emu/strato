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
import androidx.fragment.app.DialogFragment
import emu.skyline.GameActivity
import emu.skyline.R
import emu.skyline.adapter.GameItem
import kotlinx.android.synthetic.main.game_dialog.*

class GameDialog() : DialogFragment() {
    var item: GameItem? = null

    constructor(item: GameItem) : this() {
        this.item = item
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return requireActivity().layoutInflater.inflate(R.layout.game_dialog, container)
    }

    override fun onActivityCreated(savedInstanceState: Bundle?) {
        super.onActivityCreated(savedInstanceState)
        if (item is GameItem) {
            game_icon.setImageBitmap(item?.icon)
            game_title.text = item?.title
            game_subtitle.text = item?.subTitle
            val shortcutManager = activity?.getSystemService(ShortcutManager::class.java)!!
            game_pin.isEnabled = shortcutManager.isRequestPinShortcutSupported
            game_pin.setOnClickListener {
                run {
                    val info = ShortcutInfo.Builder(context, item?.title)
                    info.setShortLabel(item?.meta?.name!!)
                    info.setActivity(ComponentName(context!!, GameActivity::class.java))
                    info.setIcon(Icon.createWithBitmap(item?.icon))
                    val intent = Intent(context, GameActivity::class.java)
                    intent.data = item?.uri
                    intent.action = Intent.ACTION_VIEW
                    info.setIntent(intent)
                    shortcutManager.requestPinShortcut(info.build(), null)
                }
            }
            game_play.setOnClickListener {
                run {
                    val intent = Intent(activity, GameActivity::class.java)
                    intent.data = item?.uri
                    startActivity(intent)
                }
            }
        } else
            activity?.supportFragmentManager?.beginTransaction()?.remove(this)?.commit()
    }
}
