/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.R
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.input.ControllerType

/**
 * This item is used to display the [type] of the currently active controller
 */
class ControllerTypeViewItem(private val type : ControllerType, private val onClick : (item : ControllerTypeViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder, position : Int) {
        val context = holder.itemView.context

        content = context.getString(R.string.controller_type)
        subContent = context.getString(type.stringRes)

        super.bind(holder, position)

        holder.itemView.setOnClickListener { onClick.invoke(this, position) }
    }
}
