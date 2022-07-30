/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import androidx.viewbinding.ViewBinding

class GenericViewHolder<out V : ViewBinding>(val binding : V) : RecyclerView.ViewHolder(binding.root)

fun View.inflater() = LayoutInflater.from(context)!!

interface ViewBindingFactory {
    fun createBinding(parent : ViewGroup) : ViewBinding
}

abstract class GenericListItem<V : ViewBinding> {
    var adapter : GenericAdapter? = null

    abstract fun getViewBindingFactory() : ViewBindingFactory

    abstract fun bind(holder : GenericViewHolder<V>, position : Int)

    /**
     * Used for filtering
     */
    open fun key() : String = ""

    open fun areItemsTheSame(other : GenericListItem<V>) = this == other

    open fun areContentsTheSame(other : GenericListItem<V>) = this == other

    open val fullSpan : Boolean = false
}

abstract class SelectableGenericListItem<V : ViewBinding> : GenericListItem<V>() {
    val selectableAdapter get() = super.adapter as SelectableGenericAdapter?
}
