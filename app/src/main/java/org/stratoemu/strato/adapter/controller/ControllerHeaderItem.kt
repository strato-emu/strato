package org.stratoemu.strato.adapter.controller

import android.view.ViewGroup
import org.stratoemu.strato.adapter.GenericListItem
import org.stratoemu.strato.adapter.GenericViewHolder
import org.stratoemu.strato.adapter.ViewBindingFactory
import org.stratoemu.strato.adapter.inflater
import org.stratoemu.strato.databinding.ControllerHeaderBinding

object ControllerHeaderBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = ControllerHeaderBinding.inflate(parent.inflater(), parent, false)
}

class ControllerHeaderItem(private val text : String) : GenericListItem<ControllerHeaderBinding>() {
    override fun getViewBindingFactory() = ControllerHeaderBindingFactory

    override fun bind(holder : GenericViewHolder<ControllerHeaderBinding>, position : Int) {
        holder.binding.root.text = text
    }

    override fun areItemsTheSame(other : GenericListItem<ControllerHeaderBinding>) = other is ControllerHeaderItem

    override fun areContentsTheSame(other : GenericListItem<ControllerHeaderBinding>) = other is ControllerHeaderItem && text == other.text
}
