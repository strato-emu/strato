/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.adapter.controller

import android.view.ViewGroup
import androidx.core.view.isGone
import org.stratoemu.strato.adapter.GenericListItem
import org.stratoemu.strato.adapter.GenericViewHolder
import org.stratoemu.strato.adapter.ViewBindingFactory
import org.stratoemu.strato.adapter.inflater
import org.stratoemu.strato.databinding.ControllerItemBinding

object ControllerBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = ControllerItemBinding.inflate(parent.inflater(), parent, false)
}

open class ControllerViewItem(var content : String = "", var subContent : String = "", private val onClick : (() -> Unit)? = null) : GenericListItem<ControllerItemBinding>() {
    private var position = -1

    override fun getViewBindingFactory() = ControllerBindingFactory

    override fun bind(holder : GenericViewHolder<ControllerItemBinding>, position : Int) {
        this.position = position
        val binding = holder.binding
        binding.textTitle.apply {
            isGone = content.isEmpty()
            text = content
        }
        binding.textAuthor.apply {
            isGone = subContent.isEmpty()
            text = subContent
        }
        onClick?.let { onClick -> binding.root.setOnClickListener { onClick.invoke() } }
    }

    fun update() = adapter?.notifyItemChanged(position)

    override fun areItemsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerViewItem

    override fun areContentsTheSame(other : GenericListItem<ControllerItemBinding>) = other is ControllerViewItem && content == other.content && subContent == other.subContent
}
