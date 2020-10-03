/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.adapter.GenericViewHolder
import emu.skyline.input.ButtonGuestEvent
import emu.skyline.input.ButtonId
import emu.skyline.input.InputManager

/**
 * This item is used to display a particular [button] mapping for the controller
 */
class ControllerButtonViewItem(private val controllerId : Int, val button : ButtonId, private val onClick : (item : ControllerButtonViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder, position : Int) {
        content = button.long?.let { holder.itemView.context.getString(it) } ?: button.toString()
        val guestEvent = ButtonGuestEvent(controllerId, button)
        subContent = InputManager.eventMap.filter { it.value is ButtonGuestEvent && it.value == guestEvent }.keys.firstOrNull()?.toString() ?: ""

        super.bind(holder, position)

        holder.itemView.setOnClickListener { onClick.invoke(this, position) }
    }
}
