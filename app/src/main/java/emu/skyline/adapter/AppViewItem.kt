/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.app.Dialog
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ImageView
import android.widget.RelativeLayout
import emu.skyline.R
import emu.skyline.data.AppItem
import kotlinx.android.synthetic.main.app_item_grid_compact.*

/**
 * This enumerates the type of layouts the menu can be in
 */
enum class LayoutType(val layoutRes : Int) {
    List(R.layout.app_item_linear),
    Grid(R.layout.app_item_grid),
    GridCompact(R.layout.app_item_grid_compact)
}

private typealias InteractionFunction = (appItem : AppItem) -> Unit

private data class AppLayoutFactory(private val layoutType : LayoutType) : GenericLayoutFactory {
    override fun createLayout(parent : ViewGroup) : View = LayoutInflater.from(parent.context).inflate(layoutType.layoutRes, parent, false)
}

class AppViewItem(var layoutType : LayoutType, private val item : AppItem, private val missingIcon : Bitmap, private val onClick : InteractionFunction, private val onLongClick : InteractionFunction) : GenericViewHolderBinder() {
    override fun getLayoutFactory() : GenericLayoutFactory = AppLayoutFactory(layoutType)

    override fun bind(holder : GenericViewHolder, position : Int) {
        holder.text_title.text = item.title
        holder.text_subtitle.text = item.subTitle ?: item.loaderResultString(holder.text_subtitle.context)

        holder.icon.setImageBitmap(item.icon ?: missingIcon)

        if (layoutType == LayoutType.List) {
            holder.icon.setOnClickListener { showIconDialog(holder.icon.context, item) }
        }

        when (layoutType) {
            LayoutType.List -> holder.itemView
            LayoutType.Grid, LayoutType.GridCompact -> holder.card_app_item_grid
        }.apply {
            setOnClickListener { onClick.invoke(item) }
            setOnLongClickListener { true.also { onLongClick.invoke(item) } }
        }
    }

    private fun showIconDialog(context : Context, appItem : AppItem) {
        val builder = Dialog(context)
        builder.requestWindowFeature(Window.FEATURE_NO_TITLE)
        builder.window!!.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        val imageView = ImageView(context)
        imageView.setImageBitmap(appItem.icon ?: missingIcon)

        builder.addContentView(imageView, RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        builder.show()
    }

    override fun toString() = item.key()
}
