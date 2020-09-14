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
import android.widget.TextView
import androidx.core.content.ContextCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R
import emu.skyline.data.AppItem

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

    /**
     * The ViewHolder used by items is used to hold the views associated with an item
     *
     * @param parent The parent view that contains all the others
     * @param icon The ImageView associated with the icon
     * @param title The TextView associated with the title
     * @param subtitle The TextView associated with the subtitle
     */
    private class ItemViewHolder(val parent : View, var icon : ImageView, var title : TextView, var subtitle : TextView, var card : View? = null) : RecyclerView.ViewHolder(parent)

    /**
     * The ViewHolder used by headers is used to hold the views associated with an headers
     *
     * @param parent The parent view that contains all the others
     * @param header The TextView associated with the header
     */
    private class HeaderViewHolder(val parent : View, var header : TextView? = null) : RecyclerView.ViewHolder(parent)

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
            ElementType.Item -> {
                ItemViewHolder(view, view.findViewById(R.id.icon), view.findViewById(R.id.text_title), view.findViewById(R.id.text_subtitle)).apply {
                    if (layoutType == LayoutType.Grid || layoutType == LayoutType.GridCompact) {
                        card = view.findViewById(R.id.app_item_grid)
                        title.isSelected = true
                    }
                }
            }

            ElementType.Header -> {
                HeaderViewHolder(view).apply {
                    header = view.findViewById(R.id.text_title)
                }
            }
        }
    }

    /**
     * This function binds the item at [position] to the supplied [holder]
     */
    override fun onBindViewHolder(holder : RecyclerView.ViewHolder, position : Int) {
        val item = getItem(position)

        if (item is AppItem && holder is ItemViewHolder) {
            holder.title.text = item.title
            holder.subtitle.text = item.subTitle ?: item.loaderResultString(holder.subtitle.context)

            holder.icon.setImageBitmap(item.icon ?: missingIcon)

            if (layoutType == LayoutType.List) {
                holder.icon.setOnClickListener { showIconDialog(item) }
            }

            when (layoutType) {
                LayoutType.List -> holder.itemView
                LayoutType.Grid, LayoutType.GridCompact -> holder.card!!
            }.apply {
                setOnClickListener { onClick.invoke(item) }
                setOnLongClickListener { true.also { onLongClick.invoke(item) } }
            }
        } else if (item is BaseHeader && holder is HeaderViewHolder) {
            holder.header!!.text = item.title
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
