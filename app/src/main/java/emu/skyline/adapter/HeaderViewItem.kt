/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import emu.skyline.R
import kotlinx.android.synthetic.main.section_item.*

private object HeaderLayoutFactory : GenericLayoutFactory {
    override fun createLayout(parent : ViewGroup) : View = LayoutInflater.from(parent.context).inflate(R.layout.section_item, parent, false)
}

class HeaderViewItem(private val text : String) : GenericListItem() {
    override fun getLayoutFactory() : GenericLayoutFactory = HeaderLayoutFactory

    override fun bind(holder : GenericViewHolder, position : Int) {
        holder.text_title.text = text
    }

    override fun toString() = ""

    override fun areItemsTheSame(other : GenericListItem) = other is HeaderViewItem && text == other.text
}
