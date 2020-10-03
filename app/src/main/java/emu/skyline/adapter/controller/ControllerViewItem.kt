/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter.controller

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.isGone
import emu.skyline.R
import emu.skyline.adapter.GenericLayoutFactory
import emu.skyline.adapter.GenericViewHolder
import emu.skyline.adapter.GenericViewHolderBinder
import kotlinx.android.synthetic.main.controller_item.*

private object ControllerLayoutFactory : GenericLayoutFactory {
    override fun createLayout(parent : ViewGroup) : View = LayoutInflater.from(parent.context).inflate(R.layout.controller_item, parent, false)
}

open class ControllerViewItem(var content : String = "", var subContent : String = "", private val onClick : (() -> Unit)? = null) : GenericViewHolderBinder() {
    private var position = -1

    override fun getLayoutFactory() : GenericLayoutFactory = ControllerLayoutFactory

    override fun bind(holder : GenericViewHolder, position : Int) {
        this.position = position
        holder.text_title.apply {
            isGone = content.isEmpty()
            text = content
        }
        holder.text_subtitle.apply {
            isGone = subContent.isEmpty()
            text = subContent
        }
        onClick?.let { onClick -> holder.itemView.setOnClickListener { onClick.invoke() } }
    }

    fun update() = adapter?.notifyItemChanged(position)
}
