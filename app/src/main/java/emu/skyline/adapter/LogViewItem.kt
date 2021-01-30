/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.content.ClipData
import android.content.ClipboardManager
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import emu.skyline.R
import kotlinx.android.synthetic.main.log_item.*

private data class LogLayoutFactory(private val compact : Boolean) : GenericLayoutFactory {
    override fun createLayout(parent : ViewGroup) : View = LayoutInflater.from(parent.context).inflate(if (compact) R.layout.log_item_compact else R.layout.log_item, parent, false)
}

data class LogViewItem(private val compact : Boolean, private val message : String, private val level : String) : GenericListItem() {
    override fun getLayoutFactory() : GenericLayoutFactory = LogLayoutFactory(compact)

    override fun bind(holder : GenericViewHolder, position : Int) {
        holder.text_title.text = message
        holder.text_subtitle?.text = level

        holder.itemView.setOnClickListener {
            it.context.getSystemService(ClipboardManager::class.java).setPrimaryClip(ClipData.newPlainText("Log Message", "$message ($level)"))
            Toast.makeText(it.context, "Copied to clipboard", Toast.LENGTH_LONG).show()
        }
    }
}
