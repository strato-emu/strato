/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.databinding.ControllerItemBinding
import emu.skyline.di.getInputManager
import emu.skyline.input.ButtonGuestEvent
import emu.skyline.input.ButtonId

/**
 * This item is used to display a particular [button] mapping for the controller
 */
class ControllerButtonViewItem(private val controllerId : Int, val button : ButtonId, private val onClick : (item : ControllerButtonViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder<ControllerItemBinding>, position : Int) {
        val binding = holder.binding
        content = button.long?.let { binding.root.context.getString(it) } ?: button.toString()
        val guestEvent = ButtonGuestEvent(controllerId, button)
        subContent = binding.root.context.getInputManager().eventMap.filter { it.value is ButtonGuestEvent && it.value == guestEvent }.keys.firstOrNull()?.toString() ?: ""

        super.bind(holder, position)

        binding.root.setOnClickListener { onClick.invoke(this, position) }
    }

    override fun areItemsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerButtonViewItem && controllerId == other.controllerId

    override fun areContentsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerButtonViewItem && button == other.button
}
