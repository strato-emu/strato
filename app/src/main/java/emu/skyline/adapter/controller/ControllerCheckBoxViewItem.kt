/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import emu.skyline.R
import emu.skyline.adapter.GenericLayoutFactory
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.adapter.GenericViewHolderBinder
import kotlinx.android.synthetic.main.controller_checkbox_item.*

private object ControllerCheckBoxLayoutFactory : GenericLayoutFactory {
    override fun createLayout(parent : ViewGroup) : View = LayoutInflater.from(parent.context).inflate(R.layout.controller_checkbox_item, parent, false)
}

class ControllerCheckBoxViewItem(var title : String, var summary : String, var checked : Boolean, private val onCheckedChange : (item : ControllerCheckBoxViewItem, position : Int) -> Unit) : GenericViewHolderBinder() {
    override fun getLayoutFactory() : GenericLayoutFactory = ControllerCheckBoxLayoutFactory

    override fun bind(holder : GenericViewHolder, position : Int) {
        holder.text_title.text = title
        holder.text_subtitle.text = summary
        holder.checkbox.isChecked = checked
        holder.itemView.setOnClickListener {
            checked = !checked
            onCheckedChange.invoke(this, position)
        }
    }
}
