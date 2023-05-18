/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Strato Team and Contributors (https://github.com/strato-emu/)
 */

package emu.skyline.adapter

import android.annotation.SuppressLint
import android.net.Uri
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import emu.skyline.databinding.SearchLocationItemBinding

object SearchLocationBindingFactory : ViewBindingFactory {
    override fun createBinding(parent : ViewGroup) = SearchLocationItemBinding.inflate(parent.inflater(), parent, false)
}

open class SearchLocationViewItem(
    var uri : Uri?,
    var onDelete : ((position : Int) -> Unit)? = null
) : SelectableGenericListItem<SearchLocationItemBinding>() {
    private var holder : GenericViewHolder<SearchLocationItemBinding>? = null
    private val adapterPosition get() = holder?.adapterPosition ?: RecyclerView.NO_POSITION

    override fun getViewBindingFactory() = SearchLocationBindingFactory

    @SuppressLint("SetTextI18n")
    override fun bind(holder : GenericViewHolder<SearchLocationItemBinding>, position : Int) {
        this.holder = holder
        val binding = holder.binding

        binding.name.text = uri?.lastPathSegment?.substringAfterLast('/')
        binding.path.text = uri?.encodedPath?.replace("%3A", ":")?.replace("%2F", "/")

        onDelete?.let { onDelete ->
            binding.deleteButton.visibility = ViewGroup.VISIBLE
            binding.deleteButton.setOnClickListener {
                val pos = adapterPosition
                if (pos == RecyclerView.NO_POSITION)
                    return@setOnClickListener
                selectableAdapter?.removeItemAt(pos)

                onDelete.invoke(pos);
            }
        } ?: run {
            binding.deleteButton.visibility = ViewGroup.GONE
        }
    }

    override fun areItemsTheSame(other : GenericListItem<SearchLocationItemBinding>) : Boolean = key() == other.key()
}
