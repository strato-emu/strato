/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import emu.skyline.R
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.databinding.ControllerItemBinding
import emu.skyline.di.getInputManager
import emu.skyline.input.GeneralType
import emu.skyline.input.JoyConLeftController

/**
 * This item is used to display general settings items regarding controller
 *
 * @param type The type of controller setting this item is displaying
 */
class ControllerGeneralViewItem(private val controllerId : Int, val type : GeneralType, private val onClick : (item : ControllerGeneralViewItem, position : Int) -> Unit) : ControllerViewItem() {
    override fun bind(holder : GenericViewHolder<ControllerItemBinding>, position : Int) {
        val binding = holder.binding
        val context = binding.root.context
        val controller = context.getInputManager().controllers[controllerId]!!

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

            GeneralType.SetupGuide -> context.getString(R.string.setup_guide_description)
        }
        super.bind(holder, position)

        binding.root.setOnClickListener { onClick.invoke(this, position) }
    }

    override fun areItemsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerGeneralViewItem && controllerId == other.controllerId

    override fun areContentsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerGeneralViewItem && type == other.type
}
