/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.R
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.input.GeneralType
import emu.skyline.input.InputManager
import emu.skyline.input.JoyConLeftController

/**
 * This item is used to display general settings items regarding controller
 *
 * @param type The type of controller setting this item is displaying
 */
class ControllerGeneralViewItem(private val controllerId : Int, val type : GeneralType, private val onClick : (item : ControllerGeneralViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder, position : Int) {
        val context = holder.itemView.context
        val controller = InputManager.controllers[controllerId]!!

        content = context.getString(type.stringRes)
        subContent = when (type) {
            GeneralType.PartnerJoyCon -> {
                val partner = (controller as JoyConLeftController).partnerId

                if (partner != null)
                    "${context.getString(R.string.controller)} #${partner + 1}"
                else
                    context.getString(R.string.none)
            }

            GeneralType.RumbleDevice -> controller.rumbleDeviceName ?: context.getString(R.string.none)
        }
        super.bind(holder, position)

        holder.itemView.setOnClickListener { onClick.invoke(this, position) }
    }
}
