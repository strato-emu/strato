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
import android.view.ViewGroup
import android.widget.Toast
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R
import emu.skyline.data.BaseItem
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.log_item.*

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
internal class LogAdapter internal constructor(val context : Context, val compact : Boolean, private val debug_level : Int, private val level_str : Array<String>) : HeaderAdapter<LogItem, BaseHeader, RecyclerView.ViewHolder>() {
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

    private class ItemViewHolder(override val containerView : View) : RecyclerView.ViewHolder(containerView), LayoutContainer

    private class HeaderViewHolder(override val containerView : View) : RecyclerView.ViewHolder(containerView), LayoutContainer

    /**
     * This function creates the view-holder of type [viewType] with the layout parent as [parent]
     */
    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) : RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(context)

        val view = when (ElementType.values()[viewType]) {
            ElementType.Item -> inflater.inflate(if (compact) R.layout.log_item_compact else R.layout.log_item, parent, false)

            ElementType.Header -> inflater.inflate(R.layout.log_item, parent, false)
        }

        return when (ElementType.values()[viewType]) {
            ElementType.Item -> {
                if (compact) {
                    ItemViewHolder(view)
                } else {
                    ItemViewHolder(view)
                }
            }

            ElementType.Header -> {
                HeaderViewHolder(view)
            }
        }
    }

    /**
     * This function binds the item at [position] to the supplied [holder]
     */
    override fun onBindViewHolder(holder : RecyclerView.ViewHolder, position : Int) {
        val item = getItem(position)

        if (item is LogItem && holder is ItemViewHolder) {
            holder.text_title.text = item.message
            holder.text_subtitle?.text = item.level

            holder.itemView.setOnClickListener {
                clipboard.setPrimaryClip(ClipData.newPlainText("Log Message", item.message + " (" + item.level + ")"))
                Toast.makeText(holder.itemView.context, "Copied to clipboard", Toast.LENGTH_LONG).show()
            }
        } else if (item is BaseHeader && holder is HeaderViewHolder) {
            holder.text_title.text = item.title
        }
    }
}
