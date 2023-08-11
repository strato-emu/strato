package emu.skyline.data

import emu.skyline.loader.AppEntry
import java.io.Serializable

@Suppress("SERIAL")
class AppItem(meta : AppEntry, private val updates : List<BaseAppItem>, private val dlcs : List<BaseAppItem>) : BaseAppItem(meta), Serializable {

    fun getEnabledDlcs() : List<BaseAppItem> {
        return dlcs.filter { it.enabled }
    }

    fun getEnabledUpdate() : BaseAppItem? {
        return updates.firstOrNull { it.enabled }
    }
}