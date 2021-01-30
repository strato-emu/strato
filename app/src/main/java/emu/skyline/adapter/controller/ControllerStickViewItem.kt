/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.R
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.input.AxisGuestEvent
import emu.skyline.input.ButtonGuestEvent
import emu.skyline.input.InputManager
import emu.skyline.input.StickId

/**
 * This item is used to display all information regarding a [stick] and it's mappings for the controller
 */
class ControllerStickViewItem(private val controllerId : Int, val stick : StickId, private val onClick : (item : ControllerStickViewItem, position : Int) -> Unit) : ControllerViewItem(stick.toString()) {
    override fun bind(holder : GenericViewHolder, position : Int) {
        val context = holder.itemView.context

        val buttonGuestEvent = ButtonGuestEvent(controllerId, stick.button)
        val button = InputManager.eventMap.filter { it.value is ButtonGuestEvent && it.value == buttonGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

        var axisGuestEvent = AxisGuestEvent(controllerId, stick.yAxis, true)
        val yAxisPlus = InputManager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

        axisGuestEvent = AxisGuestEvent(controllerId, stick.yAxis, false)
        val yAxisMinus = InputManager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

        axisGuestEvent = AxisGuestEvent(controllerId, stick.xAxis, true)
        val xAxisPlus = InputManager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

        axisGuestEvent = AxisGuestEvent(controllerId, stick.xAxis, false)
        val xAxisMinus = InputManager.eventMap.filter { it.value is AxisGuestEvent && it.value == axisGuestEvent }.keys.firstOrNull()?.toString() ?: context.getString(R.string.none)

        subContent = "${context.getString(R.string.button)}: $button\n${context.getString(R.string.up)}: $yAxisPlus\n${context.getString(R.string.down)}: $yAxisMinus\n${context.getString(R.string.left)}: $xAxisMinus\n${context.getString(R.string.right)}: $xAxisPlus"

        super.bind(holder, position)

        holder.itemView.setOnClickListener { onClick.invoke(this, position) }
    }

    override fun areItemsTheSame(other : GenericListItem) = other is ControllerStickViewItem && controllerId == other.controllerId

    override fun areContentsTheSame(other : GenericListItem) = other is ControllerStickViewItem && stick == other.stick
}
