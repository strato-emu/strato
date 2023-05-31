package skyline.strato.adapter.controller

import android.view.ViewGroup
import skyline.strato.adapter.GenericListItem
import skyline.strato.adapter.GenericViewHolder
import skyline.strato.adapter.ViewBindingFactory
import skyline.strato.adapter.inflater
import skyline.strato.databinding.ControllerHeaderBinding

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
