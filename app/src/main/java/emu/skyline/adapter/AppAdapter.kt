/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package emu.skyline.adapter

import android.app.Dialog
import android.content.Context
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ImageView
import android.widget.RelativeLayout
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R
import emu.skyline.data.AppItem
import kotlinx.android.extensions.LayoutContainer
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

/**
 * This adapter is used to display all found applications using their metadata
 */
internal class AppAdapter(val layoutType : LayoutType, private val onClick : InteractionFunction, private val onLongClick : InteractionFunction) : HeaderAdapter<AppItem, BaseHeader, RecyclerView.ViewHolder>() {
    private lateinit var context : Context
    private val missingIcon by lazy { ContextCompat.getDrawable(context, R.drawable.default_icon)!!.toBitmap(256, 256) }

    /**
     * This adds a header to the view with the contents of [string]
     */
    fun addHeader(string : String) {
        super.addHeader(BaseHeader(string))
    }

    private class ItemViewHolder(override val containerView : View) : RecyclerView.ViewHolder(containerView), LayoutContainer

    private class HeaderViewHolder(override val containerView : View) : RecyclerView.ViewHolder(containerView), LayoutContainer

    /**
     * This function creates the view-holder of type [viewType] with the layout parent as [parent]
     */
    override fun onCreateViewHolder(parent : ViewGroup, viewType : Int) : RecyclerView.ViewHolder {
        context = parent.context

        val inflater = LayoutInflater.from(context)
        val view = when (ElementType.values()[viewType]) {
            ElementType.Item -> inflater.inflate(layoutType.layoutRes, parent, false)

            ElementType.Header -> inflater.inflate(R.layout.section_item, parent, false)
        }

        return when (ElementType.values()[viewType]) {
            ElementType.Item -> ItemViewHolder(view)

            ElementType.Header -> HeaderViewHolder(view)
        }
    }

    /**
     * This function binds the item at [position] to the supplied [holder]
     */
    override fun onBindViewHolder(holder : RecyclerView.ViewHolder, position : Int) {
        val item = getItem(position)

        if (item is AppItem && holder is ItemViewHolder) {
            holder.text_title.text = item.title
            holder.text_subtitle.text = item.subTitle ?: item.loaderResultString(holder.text_subtitle.context)

            holder.icon.setImageBitmap(item.icon ?: missingIcon)

            if (layoutType == LayoutType.List) {
                holder.icon.setOnClickListener { showIconDialog(item) }
            }

            when (layoutType) {
                LayoutType.List -> holder.itemView
                LayoutType.Grid, LayoutType.GridCompact -> holder.card_app_item_grid
            }.apply {
                setOnClickListener { onClick.invoke(item) }
                setOnLongClickListener { true.also { onLongClick.invoke(item) } }
            }
        } else if (item is BaseHeader && holder is HeaderViewHolder) {
            holder.text_title.text = item.title
        }
    }

    private fun showIconDialog(appItem : AppItem) {
        val builder = Dialog(context)
        builder.requestWindowFeature(Window.FEATURE_NO_TITLE)
        builder.window!!.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

        val imageView = ImageView(context)
        imageView.setImageBitmap(appItem.icon ?: missingIcon)

        builder.addContentView(imageView, RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        builder.show()
    }
}
