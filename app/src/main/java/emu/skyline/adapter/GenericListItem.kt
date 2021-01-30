/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.extensions.LayoutContainer

class GenericViewHolder(override val containerView : View) : RecyclerView.ViewHolder(containerView), LayoutContainer

interface GenericLayoutFactory {
    fun createLayout(parent : ViewGroup) : View
}

abstract class GenericListItem {
    var adapter : GenericAdapter? = null

    abstract fun getLayoutFactory() : GenericLayoutFactory

    abstract fun bind(holder : GenericViewHolder, position : Int)

    /**
     * Used for filtering
     */
    open fun key() : String = ""

    open fun areItemsTheSame(other : GenericListItem) = this == other

    /**
     * Will only be called when [areItemsTheSame] returns true, thus returning true by default
     */
    open fun areContentsTheSame(other : GenericListItem) = true
}
