/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import android.view.ViewGroup
import androidx.core.view.isGone
import emu.skyline.adapter.GenericListItem
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.adapter.ViewBindingFactory
import emu.skyline.adapter.inflater
import emu.skyline.databinding.ControllerItemBinding
import emu.skyline.input.InputManager

object ControllerBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = ControllerItemBinding.inflate(parent.inflater(), parent, false)
}

open class ControllerViewItem(var content : String = "", var subContent : String = "", private val onClick : (() -> Unit)? = null) : GenericListItem<ControllerItemBinding>() {
    private var position = -1

    override fun getViewBindingFactory() = ControllerBindingFactory

    override fun bind(binding : ControllerItemBinding, position : Int) {
        this.position = position
        binding.textTitle.apply {
            isGone = content.isEmpty()
            text = content
        }
        binding.textSubtitle.apply {
            isGone = subContent.isEmpty()
            text = subContent
        }
        onClick?.let { onClick -> binding.root.setOnClickListener { onClick.invoke() } }
    }

    fun update() = adapter?.notifyItemChanged(position)

    override fun areItemsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerViewItem

    override fun areContentsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerViewItem && content == other.content && subContent == other.subContent
}
