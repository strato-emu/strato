/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

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
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R

/**
 * This class is used to hold all data about a log entry
 */
internal class LogItem(val message : String, val level : String) : BaseItem() {
    /**
     * The log message itself is used as the search key
     */
    override fun key() : String? {
        return message
    }
}

/**
 * This adapter is used for displaying logs outputted by the application
 */
internal class LogAdapter internal constructor(val context : Context, val compact : Boolean, private val debug_level : Int, private val level_str : Array<String>) : HeaderAdapter<LogItem, BaseHeader, RecyclerView.ViewHolder>(), OnLongClickListener {
    private val clipboard : ClipboardManager = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager

    /**
     * This function adds a line to this log adapter
     */
    fun add(logLine : String) {
        try {
            val logMeta = logLine.split("|", limit = 3)

            if (logMeta[0].startsWith("1")) {
                val level = logMeta[1].toInt()
                if (level > debug_level) return

                addItem(LogItem(logMeta[2].replace('\\', '\n'), level_str[level]))
            } else {
                addHeader(BaseHeader(logMeta[1]))
            }
        } catch (ignored : IndexOutOfBoundsException) {
        } catch (ignored : NumberFormatException) {
        }
    }

    /**
     * The ViewHolder used by items is used to hold the views associated with an item
     *
     * @param parent The parent view that contains all the others
     * @param title The TextView associated with the title
     * @param subtitle The TextView associated with the subtitle
     */
    private class ItemViewHolder(val parent : View, var title : TextView, var subtitle : TextView? = null) : RecyclerView.ViewHolder(parent)

    /**
     * The ViewHolder used by headers is used to hold the views associated with an headers
     *
     * @param parent The parent view that contains all the others
     * @param header The TextView associated with the header
     */
    private class HeaderViewHolder(val parent : View, var header : TextView) : RecyclerView.ViewHolder(parent)

    /**
     * The onLongClick handler for the supplied [view], used to copy a log into the clipboard
     */
    override fun onLongClick(view : View) : Boolean {
        val item = view.tag as LogItem
        clipboard.setPrimaryClip(ClipData.newPlainText("Log Message", item.message + " (" + item.level + ")"))
        Toast.makeText(view.context, "Copied to clipboard", Toast.LENGTH_LONG).show()
        return false
    }

    /**
     * This function creates the view-holder of type [viewType] with the layout parent as [parent]
     */
    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) : RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(context)
        var holder : RecyclerView.ViewHolder? = null

        if (viewType == ElementType.Item.ordinal) {
            if (compact) {
                val view = inflater.inflate(R.layout.log_item_compact, parent, false)
                holder = ItemViewHolder(view, view.findViewById(R.id.text_title))

                view.setOnLongClickListener(this)
            } else {
                val view = inflater.inflate(R.layout.log_item, parent, false)
                holder = ItemViewHolder(view, view.findViewById(R.id.text_title), view.findViewById(R.id.text_subtitle))

                view.setOnLongClickListener(this)
            }
        } else if (viewType == ElementType.Header.ordinal) {
            val view = inflater.inflate(R.layout.section_item, parent, false)
            holder = HeaderViewHolder(view, view.findViewById(R.id.text_title))
        }

        return holder!!
    }

    /**
     * This function binds the item at [position] to the supplied [viewHolder]
     */
    override fun onBindViewHolder(viewHolder : RecyclerView.ViewHolder, position : Int) {
        val item = getItem(position)

        if (item is LogItem) {
            val holder = viewHolder as ItemViewHolder

            holder.title.text = item.message
            holder.subtitle?.text = item.level

            holder.parent.tag = item
        } else if (item is BaseHeader) {
            val holder = viewHolder as HeaderViewHolder

            holder.header.text = item.title
        }
    }
}
