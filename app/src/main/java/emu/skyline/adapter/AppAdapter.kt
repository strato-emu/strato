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
import android.net.Uri
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ImageView
import android.widget.RelativeLayout
import android.widget.TextView
import androidx.core.graphics.drawable.toBitmap
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.R
import emu.skyline.adapter.ElementType.Header
import emu.skyline.adapter.ElementType.Item
import emu.skyline.loader.AppEntry

/**
 * This class is a wrapper around [AppEntry], it is used for passing around game metadata
 */
class AppItem(val meta: AppEntry) : BaseItem() {
    /**
     * The icon of the application
     */
    val icon: Bitmap?
        get() = meta.icon

    /**
     * The title of the application
     */
    val title: String
        get() = meta.name

    /**
     * The string used as the sub-title, we currently use the author
     */
    val subTitle: String?
        get() = meta.author

    /**
     * The URI of the application's image file
     */
    val uri: Uri
        get() = meta.uri

    /**
     * The format of the application ROM as a string
     */
    private val type: String
        get() = meta.format.name

    /**
     * The name and author is used as the key
     */
    override fun key(): String? {
        return if (meta.author != null) meta.name + " " + meta.author else meta.name
    }
}

/**
 * This enumerates the type of layouts the menu can be in
 */
enum class LayoutType {
    List,
    Grid,
}

/**
 * This adapter is used to display all found applications using their metadata
 */
internal class AppAdapter(val context: Context?, private val layoutType: LayoutType) : HeaderAdapter<AppItem, BaseHeader, RecyclerView.ViewHolder>(), View.OnClickListener {
    private val missingIcon = context?.resources?.getDrawable(R.drawable.default_icon, context.theme)?.toBitmap(256, 256)
    private val missingString = context?.getString(R.string.metadata_missing)

    /**
     * This adds a header to the view with the contents of [string]
     */
    fun addHeader(string: String) {
        super.addHeader(BaseHeader(string))
    }

    /**
     * The onClick handler for the supplied [view], used for the icon preview
     */
    override fun onClick(view: View) {
        val position = view.tag as Int

        if (getItem(position) is AppItem) {
            val item = getItem(position) as AppItem

            if (view.id == R.id.icon) {
                val builder = Dialog(context!!)
                builder.requestWindowFeature(Window.FEATURE_NO_TITLE)
                builder.window!!.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))

                val imageView = ImageView(context)
                imageView.setImageBitmap(item.icon ?: missingIcon)

                builder.addContentView(imageView, RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
                builder.show()
            }
        }
    }

    /**
     * The ViewHolder used by items is used to hold the views associated with an item
     *
     * @param parent The parent view that contains all the others
     * @param icon The ImageView associated with the icon
     * @param title The TextView associated with the title
     * @param subtitle The TextView associated with the subtitle
     */
    private class ItemViewHolder(val parent: View, var icon: ImageView, var title: TextView, var subtitle: TextView, var card: View? = null) : RecyclerView.ViewHolder(parent)

    /**
     * The ViewHolder used by headers is used to hold the views associated with an headers
     *
     * @param parent The parent view that contains all the others
     * @param header The TextView associated with the header
     */
    private class HeaderViewHolder(val parent: View, var header: TextView? = null) : RecyclerView.ViewHolder(parent)

    /**
     * This function creates the view-holder of type [viewType] with the layout parent as [parent]
     */
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): RecyclerView.ViewHolder {
        val inflater = LayoutInflater.from(context)
        var holder: RecyclerView.ViewHolder? = null

        if (viewType == Item.ordinal) {
            val view = inflater.inflate(if (layoutType == LayoutType.List) R.layout.app_item_linear else R.layout.app_item_grid, parent, false)
            holder = ItemViewHolder(view, view.findViewById(R.id.icon), view.findViewById(R.id.text_title), view.findViewById(R.id.text_subtitle))

            if (layoutType == LayoutType.List) {
                if (context is View.OnClickListener)
                    view.setOnClickListener(context as View.OnClickListener)

                if (context is View.OnLongClickListener)
                    view.setOnLongClickListener(context as View.OnLongClickListener)
            } else {
                holder.card = view.findViewById(R.id.app_item_grid)

                if (context is View.OnClickListener)
                    holder.card!!.setOnClickListener(context as View.OnClickListener)

                if (context is View.OnLongClickListener)
                    holder.card!!.setOnLongClickListener(context as View.OnLongClickListener)
            }
        } else if (viewType == Header.ordinal) {
            val view = inflater.inflate(R.layout.section_item, parent, false)
            holder = HeaderViewHolder(view)

            holder.header = view.findViewById(R.id.text_title)
        }

        return holder!!
    }

    /**
     * This function binds the item at [position] to the supplied [viewHolder]
     */
    override fun onBindViewHolder(viewHolder: RecyclerView.ViewHolder, position: Int) {
        val item = getItem(position)

        if (item is AppItem) {
            val holder = viewHolder as ItemViewHolder

            holder.title.text = item.title
            holder.subtitle.text = item.subTitle ?: missingString

            holder.icon.setImageBitmap(item.icon ?: missingIcon)

            if (layoutType == LayoutType.List) {
                holder.icon.setOnClickListener(this)
                holder.icon.tag = position
            }

            holder.card?.tag = item
            holder.parent.tag = item
        } else if (item is BaseHeader) {
            val holder = viewHolder as HeaderViewHolder

            holder.header!!.text = item.title
        }
    }
}
