/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.adapter.controller

import skyline.strato.R
import skyline.strato.adapter.GenericListItem
import skyline.strato.adapter.GenericViewHolder
import skyline.strato.databinding.ControllerItemBinding
import skyline.strato.di.getInputManager
import skyline.strato.input.GeneralType
import skyline.strato.input.JoyConLeftController

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
