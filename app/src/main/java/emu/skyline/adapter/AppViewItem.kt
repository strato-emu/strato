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
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ImageView
import android.widget.RelativeLayout
import android.widget.TextView
import androidx.viewbinding.ViewBinding
import emu.skyline.R
import emu.skyline.data.AppItem
import emu.skyline.databinding.AppItemGridBinding
import emu.skyline.databinding.AppItemGridCompactBinding
import emu.skyline.databinding.AppItemLinearBinding

sealed class LayoutType(val builder : (parent : ViewGroup) -> ViewBinding) {
    object List : LayoutType({ ListBinding(it) })
    object Grid : LayoutType({ GridBinding(it) })
    object GridCompact : LayoutType({ GridCompatBinding(it) })

    companion object {
        fun values() = arrayListOf(List, Grid, GridCompact)
    }
}

data class LayoutBindingFactory(private val layoutType : LayoutType) : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = layoutType.builder(parent)
}

interface LayoutBinding<V : ViewBinding> : ViewBinding {
    val binding : V

    override fun getRoot() = binding.root

    val textTitle : TextView

    val textSubtitle : TextView

    val icon : ImageView
}

class ListBinding(parent : ViewGroup) : LayoutBinding<AppItemLinearBinding> {
    override val binding = AppItemLinearBinding.inflate(parent.inflater(), parent, false)

    override val textTitle = binding.textTitle

    override val textSubtitle = binding.textSubtitle

    override val icon = binding.icon
}

class GridBinding(parent : ViewGroup) : LayoutBinding<AppItemGridBinding> {
    override val binding = AppItemGridBinding.inflate(parent.inflater(), parent, false)

    override val textTitle = binding.textTitle

    override val textSubtitle = binding.textSubtitle

    override val icon = binding.icon
}

class GridCompatBinding(parent : ViewGroup) : LayoutBinding<AppItemGridCompactBinding> {
    override val binding = AppItemGridCompactBinding.inflate(parent.inflater(), parent, false)

    override val textTitle = binding.textTitle

    override val textSubtitle = binding.textSubtitle

    override val icon = binding.icon
}

private typealias InteractionFunction = (appItem : AppItem) -> Unit

class AppViewItem(var layoutType : LayoutType, private val item : AppItem, private val missingIcon : Bitmap, private val onClick : InteractionFunction, private val onLongClick : InteractionFunction) : GenericListItem<LayoutBinding<*>>() {
    override fun getViewBindingFactory() = LayoutBindingFactory(layoutType)

    override fun bind(binding : LayoutBinding<*>, position : Int) {
        binding.textTitle.text = item.title
        binding.textSubtitle.text = item.subTitle ?: item.loaderResultString(binding.root.context)

        binding.icon.setImageBitmap(item.icon ?: missingIcon)

        if (layoutType == LayoutType.List) {
            binding.icon.setOnClickListener { showIconDialog(it.context, item) }
        }

        binding.root.findViewById<View>(R.id.item_click_layout).apply {
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

    override fun key() = item.key()

    override fun areItemsTheSame(other : GenericListItem<LayoutBinding<*>>) = key() == other.key()

    override fun areContentsTheSame(other : GenericListItem<LayoutBinding<*>>) = other is AppViewItem && layoutType == other.layoutType && item == other.item
}
