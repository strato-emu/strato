/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.R
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.databinding.ControllerItemBinding
import emu.skyline.input.ControllerType

/**
 * This item is used to display the [type] of the currently active controller
 */
class ControllerTypeViewItem(private val type : ControllerType, private val onClick : (item : ControllerTypeViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder<ControllerItemBinding>, position : Int) {
        val binding = holder.binding
        val context = binding.root.context

        content = context.getString(R.string.controller_type)
        subContent = context.getString(type.stringRes)

        super.bind(holder, position)

        binding.root.setOnClickListener { onClick.invoke(this, position) }
    }

    override fun areItemsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerTypeViewItem

    override fun areContentsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerTypeViewItem && type == other.type
}
