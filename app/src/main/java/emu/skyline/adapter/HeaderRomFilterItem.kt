/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.ViewGroup
import com.google.android.material.chip.Chip
import emu.skyline.R
import emu.skyline.databinding.HeaderRomFilterBinding
import emu.skyline.loader.RomFormat

object HeaderRomFilterBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = HeaderRomFilterBinding.inflate(parent.inflater(), parent, false)
}

typealias OnFilterClickedListener = (format : RomFormat?) -> Unit

class HeaderRomFilterItem(private val formats : List<RomFormat>, selectedFormat : RomFormat?, private val onFilterClickedListener : OnFilterClickedListener) : GenericListItem<HeaderRomFilterBinding>() {
    private var selection = selectedFormat?.let { formats.indexOf(it) + 1 } ?: 0

    override fun getViewBindingFactory() = HeaderRomFilterBindingFactory

    override fun bind(binding : HeaderRomFilterBinding, position : Int) {
        binding.chipGroup.removeViews(1, binding.chipGroup.childCount - 1)
        for (format in formats) {
            binding.chipGroup.addView(Chip(binding.root.context, null, R.attr.chipChoiceStyle).apply { text = format.name })
        }
        binding.chipGroup.setOnCheckedChangeListener { group, checkedId ->
            for (i in 0 until group.childCount) {
                if (group.getChildAt(i).id == checkedId) {
                    selection = i
                    onFilterClickedListener(if (i == 0) null else formats[i - 1])
                    break
                }
            }
        }
        binding.chipGroup.check(binding.chipGroup.getChildAt(selection).id)
    }

    override val fullSpan = true
}
