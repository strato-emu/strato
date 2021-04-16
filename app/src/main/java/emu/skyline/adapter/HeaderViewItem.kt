/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.ViewGroup
import emu.skyline.databinding.SectionItemBinding

object HeaderBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = SectionItemBinding.inflate(parent.inflater(), parent, false)
}

class HeaderViewItem(private val text : String) : GenericListItem<SectionItemBinding>() {
    override fun getViewBindingFactory() = HeaderBindingFactory

    override fun bind(binding : SectionItemBinding, position : Int) {
        binding.textTitle.text = text
    }

    override fun toString() = ""

    override fun areItemsTheSame(other : GenericListItem<SectionItemBinding>) = other is HeaderViewItem && text == other.text

    override val fullSpan = true
}
