/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.adapter.controller

import skyline.strato.R
import skyline.strato.adapter.GenericListItem
import skyline.strato.adapter.GenericViewHolder
import skyline.strato.databinding.ControllerItemBinding
import skyline.strato.input.ControllerType

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
