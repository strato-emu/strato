package emu.skyline.adapter

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.view.LayoutInflater
import android.view.View
import android.view.View.OnLongClickListener
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import emu.skyline.R

internal class LogItem(val message: String, val level: String) : BaseItem() {
    override fun key(): String? {
        return message
    }
}

internal class LogAdapter internal constructor(val context: Context, val compact: Boolean, private val debug_level: Int, private val level_str: Array<String>) : HeaderAdapter<LogItem, BaseHeader>(), OnLongClickListener {
    private val clipboard: ClipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    fun add(logLine: String) {
        try {
            val logMeta = logLine.split("|", limit = 3)
            if (logMeta[0].startsWith("1")) {
                val level = logMeta[1].toInt()
                if (level > debug_level) return
                addItem(LogItem(logMeta[2].replace('\\', '\n'), level_str[level]))
            } else {
                addHeader(BaseHeader(logMeta[1]))
            }
        } catch (ignored: IndexOutOfBoundsException) {
        }
    }

    override fun onLongClick(view: View): Boolean {
        val item = getItem((view.tag as ViewHolder).position) as LogItem
        clipboard.setPrimaryClip(ClipData.newPlainText("Log Message", item.message + " (" + item.level + ")"))
        Toast.makeText(view.context, "Copied to clipboard", Toast.LENGTH_LONG).show()
        return false
    }

    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
        var view = convertView
        val viewHolder: ViewHolder
        val item = elementArray[visibleArray[position]]
        if (view == null) {
            viewHolder = ViewHolder()
            val inflater = LayoutInflater.from(context)
            if (item is LogItem) {
                if (compact) {
                    view = inflater.inflate(R.layout.log_item_compact, parent, false)
                    viewHolder.txtTitle = view.findViewById(R.id.text_title)
                } else {
                    view = inflater.inflate(R.layout.log_item, parent, false)
                    viewHolder.txtTitle = view.findViewById(R.id.text_title)
                    viewHolder.txtSub = view.findViewById(R.id.text_subtitle)
                }
                view.setOnLongClickListener(this)
            } else if (item is BaseHeader) {
                view = inflater.inflate(R.layout.section_item, parent, false)
                viewHolder.txtTitle = view.findViewById(R.id.text_title)
            }
            view!!.tag = viewHolder
        } else {
            viewHolder = view.tag as ViewHolder
        }
        if (item is LogItem) {
            viewHolder.txtTitle!!.text = item.message
            if (!compact) viewHolder.txtSub!!.text = item.level
        } else if (item is BaseHeader) {
            viewHolder.txtTitle!!.text = item.title
        }
        viewHolder.position = position
        return view
    }

    private class ViewHolder {
        var txtTitle: TextView? = null
        var txtSub: TextView? = null
        var position = 0
    }

}
