/*
 * SPDX-License-Identifier: LGPL-3.0-or-later
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
import emu.skyline.R
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
        get() = meta.name + " (" + type + ")"

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

    override fun key(): String? {
        return if (meta.author != null) meta.name + " " + meta.author else meta.name
    }
}

/**
 * This adapter is used to display all found applications using their metadata
 */
internal class AppAdapter(val context: Context?) : HeaderAdapter<AppItem, BaseHeader>(), View.OnClickListener {
    /**
     * This adds a string header to the view
     */
    fun addHeader(string: String) {
        super.addHeader(BaseHeader(string))
    }

    /**
     * The onClick handler, it's for displaying the icon preview
     *
     * @param view The specific view that was clicked
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
                imageView.setImageBitmap(item.icon)
                builder.addContentView(imageView, RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
                builder.show()
            }
        }
    }

    /**
     * This returns the view for an element at a specific position
     *
     * @param position The position of the requested item
     * @param convertView An existing view (If any)
     * @param parent The parent view group used for layout inflation
     */
    override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
        var view = convertView
        val viewHolder: ViewHolder
        val item = elementArray[visibleArray[position]]

        if (view == null) {
            viewHolder = ViewHolder()
            if (item is AppItem) {
                val inflater = LayoutInflater.from(context)
                view = inflater.inflate(R.layout.game_item, parent, false)

                viewHolder.icon = view.findViewById(R.id.icon)
                viewHolder.title = view.findViewById(R.id.text_title)
                viewHolder.subtitle = view.findViewById(R.id.text_subtitle)
                view.tag = viewHolder
            } else if (item is BaseHeader) {
                val inflater = LayoutInflater.from(context)
                view = inflater.inflate(R.layout.section_item, parent, false)

                viewHolder.title = view.findViewById(R.id.text_title)
                view.tag = viewHolder
            }
        } else {
            viewHolder = view.tag as ViewHolder
        }

        if (item is AppItem) {
            val data = getItem(position) as AppItem

            viewHolder.title!!.text = data.title
            viewHolder.subtitle!!.text = data.subTitle ?: context?.getString(R.string.metadata_missing)!!

            viewHolder.icon!!.setImageBitmap(data.icon ?: context!!.resources.getDrawable(R.drawable.ic_missing, context.theme).toBitmap(256, 256))
            viewHolder.icon!!.setOnClickListener(this)
            viewHolder.icon!!.tag = position
        } else {
            viewHolder.title!!.text = (getItem(position) as BaseHeader).title
        }

        return view!!
    }

    /**
     * The ViewHolder object is used to hold the views associated with an object
     *
     * @param icon The ImageView associated with the icon
     * @param title The TextView associated with the title
     * @param subtitle The TextView associated with the subtitle
     */
    private class ViewHolder(var icon: ImageView? = null, var title: TextView? = null, var subtitle: TextView? = null)
}
