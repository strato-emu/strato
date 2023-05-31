/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package skyline.strato.adapter.controller

import android.view.ViewGroup
import androidx.core.view.isGone
import skyline.strato.adapter.GenericListItem
import skyline.strato.adapter.GenericViewHolder
import skyline.strato.adapter.ViewBindingFactory
import skyline.strato.adapter.inflater
import skyline.strato.databinding.ControllerCheckboxItemBinding

object ControllerCheckBoxBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = ControllerCheckboxItemBinding.inflate(parent.inflater(), parent, false)
}

class ControllerCheckBoxViewItem(var title : String, var summary : String, var checked : Boolean, private val onCheckedChange : (item : ControllerCheckBoxViewItem, position : Int) -> Unit) : GenericListItem<ControllerCheckboxItemBinding>() {
    override fun getViewBindingFactory() = ControllerCheckBoxBindingFactory

    override fun bind(holder : GenericViewHolder<ControllerCheckboxItemBinding>, position : Int) {
        val binding = holder.binding
        binding.textTitle.isGone = title.isEmpty()
        binding.textTitle.text = title
        binding.textAuthor.isGone = summary.isEmpty()
        binding.textAuthor.text = summary
        binding.checkbox.isChecked = checked
        binding.root.setOnClickListener {
            checked = !checked
            binding.checkbox.isChecked = checked
            onCheckedChange.invoke(this, position)
        }
    }

    override fun areItemsTheSame(other : GenericListItem<ControllerCheckboxItemBinding>) = other is ControllerCheckBoxViewItem

    override fun areContentsTheSame(other : GenericListItem<ControllerCheckboxItemBinding>) = other is ControllerCheckBoxViewItem && title == other.title && summary == other.summary && checked == other.checked
}
