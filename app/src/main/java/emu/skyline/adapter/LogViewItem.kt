/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.content.ClipData
import android.content.ClipboardManager
import android.view.ViewGroup
import android.widget.TextView
import android.widget.Toast
import androidx.viewbinding.ViewBinding
import emu.skyline.databinding.LogItemBinding
import emu.skyline.databinding.LogItemCompactBinding

data class LogBindingFactory(private val compact : Boolean) : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = if (compact) LogCompactBinding(parent) else LogBinding(parent)
}

interface ILogBinding : ViewBinding {
    val binding : ViewBinding

    override fun getRoot() = binding.root

    val textTitle : TextView

    val textSubTitle : TextView?
}

class LogCompactBinding(parent : ViewGroup) : ILogBinding {
    override val binding = LogItemCompactBinding.inflate(parent.inflater(), parent, false)

    override val textTitle = binding.textTitle

    override val textSubTitle : Nothing? = null
}

class LogBinding(parent : ViewGroup) : ILogBinding {
    override val binding = LogItemBinding.inflate(parent.inflater(), parent, false)

    override val textTitle = binding.textTitle

    override val textSubTitle = binding.textSubtitle
}

data class LogViewItem(private val compact : Boolean, private val message : String, private val level : String) : GenericListItem<ILogBinding>() {
    override fun getViewBindingFactory() = LogBindingFactory(compact)

    override fun bind(binding : ILogBinding, position : Int) {
        binding.textTitle.text = message
        binding.textSubTitle?.text = level

        binding.root.setOnClickListener {
            it.context.getSystemService(ClipboardManager::class.java).setPrimaryClip(ClipData.newPlainText("Log Message", "$message ($level)"))
            Toast.makeText(it.context, "Copied to clipboard", Toast.LENGTH_LONG).show()
        }
    }
}
